#!/usr/bin/env python3
"""
Echo Grid — closed-loop CSI dashboard

  python visualization/dashboard.py --csi

Listens UDP :4210, visualizes nodes, sends echo_cmd on :4211.
Auth secret is baked in to match firmware (override with --secret / ECHO_SECRET).
"""
from __future__ import annotations

import argparse
import json
import math
import os
import socket
import sys
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

CSI_PORT = 4210
CMD_PORT = 4211

# Must match esp32/include/echo_secret.h
DEFAULT_SECRET = "Eg7$kQ2mN9pR4vX8wL3hJ6cF1bA5yU0zT"


def auth_tag(secret: str, node: str, ts: int, bucket_ms: int = 60000) -> str:
    if not secret:
        return ""
    bucket = int(ts) // bucket_ms
    h = 2166136261
    data = f"{secret}|{node}|{bucket}".encode("utf-8")
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return f"{h:08x}"


def auth_ok(secret: str, pkt: dict) -> bool:
    if not secret:
        return True
    node = str(pkt.get("node", ""))
    ts = int(pkt.get("timestamp") or 0)
    got = str(pkt.get("auth") or "")
    if not got:
        return False
    for skew in (0, -1, 1):
        expect = auth_tag(secret, node, ts + skew * 60000)
        if expect == got:
            return True
    # wall-clock fallback for mixed timestamp domains
    now_ms = int(time.time() * 1000)
    for skew in (0, -1, 1):
        if auth_tag(secret, node, now_ms + skew * 60000) == got:
            return True
    return False


@dataclass
class NodeState:
    node: str
    last_seen: float = 0.0
    rssi: float = -100
    activity: float = 0.0
    movement: float = 0.0
    channel: int = 0
    csi: List[float] = field(default_factory=lambda: [0.0] * 32)
    via: str = "udp"
    addr: Optional[Tuple[str, int]] = None
    packets: int = 0


class EchoGrid:
    def __init__(self, secret: str = DEFAULT_SECRET, cmd_port: int = CMD_PORT):
        self.secret = secret
        self.cmd_port = cmd_port
        self.nodes: Dict[str, NodeState] = {}
        self.history: deque = deque(maxlen=120)
        self.rejected = 0
        self.accepted = 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind(("0.0.0.0", CSI_PORT))
        self.sock.settimeout(0.25)
        self.cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.cmd_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        print(f"[EchoGrid] CSI UDP :{CSI_PORT}  auth={'on' if secret else 'off'}")

    def ingest(self, pkt: dict, addr: Tuple[str, int]) -> None:
        if pkt.get("type") != "wifi_csi":
            return
        if not auth_ok(self.secret, pkt):
            self.rejected += 1
            return
        self.accepted += 1
        node = str(pkt.get("node", "unknown"))
        st = self.nodes.get(node) or NodeState(node=node)
        st.last_seen = time.time()
        st.rssi = float(pkt.get("rssi", st.rssi))
        st.activity = float(pkt.get("activity", st.activity))
        st.movement = float(pkt.get("movement_intensity", st.movement))
        st.channel = int(pkt.get("channel", st.channel))
        csi = pkt.get("csi") or st.csi
        if isinstance(csi, list) and len(csi) >= 8:
            st.csi = [float(x) for x in csi[:32]]
            while len(st.csi) < 32:
                st.csi.append(0.0)
        st.via = str(pkt.get("via", "udp"))
        st.addr = addr
        st.packets += 1
        self.nodes[node] = st
        self.history.append((time.time(), node, st.activity, st.movement))

    def live_nodes(self, max_age: float = 5.0) -> List[NodeState]:
        now = time.time()
        return [n for n in self.nodes.values() if now - n.last_seen <= max_age]

    def field_stats(self) -> dict:
        nodes = self.live_nodes()
        if not nodes:
            return {
                "entropy": 0.0, "tracks": 0, "motion": 0.0, "df_max": 0.0,
                "nodes": 0, "bands": 0, "agreed": False,
            }
        acts = [n.activity for n in nodes]
        movs = [n.movement for n in nodes]
        mean = sum(acts) / len(acts)
        var = sum((a - mean) ** 2 for a in acts) / max(len(acts), 1)
        entropy = min(1.5, math.sqrt(var) * 3.0 + mean * 0.5)
        tracks = sum(1 for a in acts if a > 0.35)
        motion = max(movs) if movs else 0.0
        channels = {n.channel for n in nodes if n.channel}
        agreed = len(nodes) >= 2 and (max(acts) - min(acts) < 0.35)
        return {
            "entropy": round(entropy, 3),
            "tracks": tracks,
            "motion": round(motion, 3),
            "df_max": round(motion * 40.0, 1),
            "nodes": len(nodes),
            "bands": len(channels) or 1,
            "agreed": agreed,
        }

    def send_echo_cmd(self, cmd: dict) -> None:
        if self.secret:
            cmd["auth"] = auth_tag(self.secret, "host", int(time.time() * 1000))
            cmd["timestamp"] = int(time.time() * 1000)
        payload = json.dumps(cmd).encode("utf-8")
        self.cmd_sock.sendto(payload, ("255.255.255.255", self.cmd_port))
        seen = set()
        for n in self.live_nodes(8.0):
            if n.addr and n.addr[0] not in seen:
                seen.add(n.addr[0])
                try:
                    self.cmd_sock.sendto(payload, (n.addr[0], self.cmd_port))
                except OSError:
                    pass

    def closed_loop_tick(self) -> None:
        fs = self.field_stats()
        self.send_echo_cmd({"type": "echo_cmd", "cmd": "field", **fs})
        if fs["motion"] > 0.45 or fs["tracks"] >= 2:
            self.send_echo_cmd({
                "type": "echo_cmd", "cmd": "boost",
                "level": min(1.0, 0.4 + fs["motion"]),
            })
        elif fs["nodes"] > 0 and fs["motion"] < 0.12:
            self.send_echo_cmd({"type": "echo_cmd", "cmd": "quiet"})

    def render(self) -> str:
        nodes = self.live_nodes(6.0)
        fs = self.field_stats()
        lines = [
            "=" * 64,
            "  ECHO GRID  —  closed-loop CSI (auth on)",
            f"  nodes={fs['nodes']}  tracks={fs['tracks']}  "
            f"motion={fs['motion']:.2f}  H={fs['entropy']:.2f}  "
            f"agreed={fs['agreed']}  ok={self.accepted} rej={self.rejected}",
            "=" * 64,
        ]
        if not nodes:
            lines.append("  (waiting for wifi_csi on UDP 4210…)")
        for n in sorted(nodes, key=lambda x: x.node):
            bar_a = "█" * int(n.activity * 20) + "░" * (20 - int(n.activity * 20))
            bar_m = "█" * int(n.movement * 20) + "░" * (20 - int(n.movement * 20))
            lines.append(
                f"  {n.node:14s} rssi={n.rssi:4.0f} ch={n.channel:2d} "
                f"via={n.via:6s} pkts={n.packets}"
            )
            lines.append(f"    activity  [{bar_a}] {n.activity:.2f}")
            lines.append(f"    movement  [{bar_m}] {n.movement:.2f}")
            spark = "".join(
                "█" if v > 0.7 else "▓" if v > 0.45 else "▒" if v > 0.25 else "░"
                for v in n.csi[::2]
            )
            lines.append(f"    csi       {spark}")
        lines.append("=" * 64)
        return "\n".join(lines)

    def poll(self) -> None:
        try:
            data, addr = self.sock.recvfrom(4096)
        except socket.timeout:
            return
        except OSError as e:
            print(f"[EchoGrid] socket error: {e}")
            return
        try:
            pkt = json.loads(data.decode("utf-8", errors="ignore"))
        except json.JSONDecodeError:
            return
        if isinstance(pkt, dict):
            self.ingest(pkt, addr)


def main() -> None:
    ap = argparse.ArgumentParser(description="Echo Grid closed-loop CSI dashboard")
    ap.add_argument("--csi", action="store_true", help="CSI mode (UDP 4210)")
    ap.add_argument(
        "--secret",
        default=os.environ.get("ECHO_SECRET", DEFAULT_SECRET),
        help="Shared auth secret (default matches firmware)",
    )
    ap.add_argument("--loop-hz", type=float, default=2.0)
    ap.add_argument("--no-loop", action="store_true")
    args = ap.parse_args()

    if not args.csi:
        print("Use: python visualization/dashboard.py --csi")
        sys.exit(0)

    grid = EchoGrid(secret=args.secret)
    last_render = 0.0
    last_loop = 0.0
    loop_period = 1.0 / max(args.loop_hz, 0.2)

    print("[EchoGrid] running — Ctrl+C to stop")
    try:
        while True:
            grid.poll()
            now = time.time()
            if not args.no_loop and now - last_loop >= loop_period:
                grid.closed_loop_tick()
                last_loop = now
            if now - last_render >= 0.5:
                sys.stdout.write("\033[2J\033[H")
                sys.stdout.write(grid.render() + "\n")
                sys.stdout.flush()
                last_render = now
    except KeyboardInterrupt:
        print("\n[EchoGrid] stopped")


if __name__ == "__main__":
    main()
