from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple
import time
import os

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class NodeState:
    node_id: str
    last_seen: float = field(default_factory=time.time)
    rssi: int = 0
    activity: float = 0.0
    hot_zones: int = 0
    obstruction: bool = False
    movement_intensity: float = 0.0      # Simple proxy for how much things are changing
    confidence: float = 0.5               # How trustworthy the current reading is (0-1)
    position: Optional[Tuple[float, float]] = None
    packet_count: int = 0
    last_activity: float = 0.0            # For calculating movement intensity


class NodeManager:
    def __init__(self, 
                 room_size: Tuple[float, float] = (12.0, 10.0), 
                 grid_resolution: int = 20,
                 nodes_yaml_path: str = "nodes.yaml"):
        self.nodes: Dict[str, NodeState] = {}
        self.room_size = room_size
        self.grid_resolution = grid_resolution
        self.probability_field = [[0.0 for _ in range(grid_resolution)] 
                                  for _ in range(grid_resolution)]
        self.known_positions: Dict[str, Tuple[float, float]] = {}

        self._load_positions_from_yaml(nodes_yaml_path)

    def _load_positions_from_yaml(self, path: str):
        if not HAS_YAML or not os.path.exists(path):
            return
        try:
            with open(path, "r") as f:
                data = yaml.safe_load(f) or {}
            for node_id, info in data.items():
                if isinstance(info, dict) and "position" in info:
                    pos = info["position"]
                    if isinstance(pos, (list, tuple)) and len(pos) == 2:
                        self.known_positions[node_id] = (float(pos[0]), float(pos[1]))
            print(f"[NodeManager] Loaded {len(self.known_positions)} node positions from {path}")
        except Exception as e:
            print(f"[NodeManager] Failed to load {path}: {e}")

    def register_or_update(self, node_id: str, data: dict):
        now = time.time()

        if node_id not in self.nodes:
            pos = self.known_positions.get(node_id)
            self.nodes[node_id] = NodeState(node_id=node_id, position=pos)

        node = self.nodes[node_id]
        prev_activity = node.activity

        node.last_seen = now
        node.rssi = data.get("rssi", node.rssi)
        node.activity = data.get("activity", node.activity)
        node.hot_zones = data.get("hot_zones", node.hot_zones)
        node.obstruction = data.get("obstruction", node.obstruction)
        node.packet_count += 1

        # Calculate movement intensity (how much activity changed)
        activity_change = abs(node.activity - prev_activity)
        node.movement_intensity = min(1.0, activity_change * 3.0 + node.activity * 0.6)

        # Simple confidence model (higher when we have recent consistent data)
        time_since_last = now - node.last_seen if node.last_seen else 10
        recency = max(0.3, 1.0 - min(time_since_last / 30.0, 0.7))
        node.confidence = min(0.95, recency * 0.7 + (node.packet_count / 50.0) * 0.3)

        self._update_probability_field()

    def _update_probability_field(self):
        for y in range(self.grid_resolution):
            for x in range(self.grid_resolution):
                self.probability_field[y][x] = 0.0

        if not self.nodes:
            return

        cell_w = self.room_size[0] / self.grid_resolution
        cell_h = self.room_size[1] / self.grid_resolution

        for node in self.nodes.values():
            if node.position is None:
                continue

            gx = int(node.position[0] / cell_w)
            gy = int(node.position[1] / cell_h)
            gx = max(0, min(self.grid_resolution - 1, gx))
            gy = max(0, min(self.grid_resolution - 1, gy))

            # Stronger influence from nodes with high movement + confidence
            influence = node.movement_intensity * node.confidence * 1.6
            influence = min(1.0, influence)

            self.probability_field[gy][gx] = max(self.probability_field[gy][gx], influence)

            # Distance-weighted spread to neighbors
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    nx = gx + dx
                    ny = gy + dy
                    if 0 <= nx < self.grid_resolution and 0 <= ny < self.grid_resolution:
                        dist = max(1, abs(dx) + abs(dy))
                        spread = influence * (1.0 / dist)
                        self.probability_field[ny][nx] = max(
                            self.probability_field[ny][nx], spread
                        )

    def get_room_state(self) -> dict:
        total_activity = sum(n.activity for n in self.nodes.values())
        total_hot_zones = sum(n.hot_zones for n in self.nodes.values())
        obstruction_count = sum(1 for n in self.nodes.values() if n.obstruction)
        obstruction_prob = min(1.0, obstruction_count / max(1, len(self.nodes)))

        return {
            "node_count": len(self.nodes),
            "total_activity": round(total_activity, 3),
            "total_hot_zones": total_hot_zones,
            "obstruction_probability": round(obstruction_prob, 3),
            "probability_field": self.probability_field,
            "nodes": {
                nid: {
                    "activity": round(n.activity, 3),
                    "hot_zones": n.hot_zones,
                    "obstruction": n.obstruction,
                    "movement_intensity": round(n.movement_intensity, 3),
                    "confidence": round(n.confidence, 3),
                    "position": n.position
                }
                for nid, n in self.nodes.items()
            }
        }

    def cleanup_stale_nodes(self, timeout_seconds: float = 45.0):
        now = time.time()
        stale = [nid for nid, n in self.nodes.items() if now - n.last_seen > timeout_seconds]
        for nid in stale:
            del self.nodes[nid]
        if stale:
            self._update_probability_field()
