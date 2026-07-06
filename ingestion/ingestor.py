import socket
import json
import logging
import time
import argparse

logger = logging.getLogger("csi-ingestor")

class CSIIngestor:
    """
    Flexible ingestor that can accept data from multiple sources:
    - WiFi CSI nodes (rich context)
    - Meshtastic gateways (lightweight summarized data)
    """

    def __init__(self, udp_port: int = 4210, max_retries: int = 5):
        self.udp_port = udp_port
        self.max_retries = max_retries
        self.sock = None
        self._backoff = 1
        self._setup_socket()

    def _setup_socket(self):
        try:
            if self.sock:
                self.sock.close()
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(("0.0.0.0", self.udp_port))
            self.sock.settimeout(0.8)
            self._backoff = 1
            logger.info(f"Listening on UDP port {self.udp_port}")
        except Exception as e:
            logger.error(f"Failed to bind UDP socket: {e}")
            self.sock = None

    def read_packet(self):
        if self.sock is None:
            self._setup_socket()
            if self.sock is None:
                time.sleep(min(self._backoff, 10))
                self._backoff = min(self._backoff * 2, 10)
                return None

        for attempt in range(self.max_retries):
            try:
                data, addr = self.sock.recvfrom(2048)
                self._backoff = 1
                return json.loads(data.decode())
            except socket.timeout:
                return None
            except Exception as e:
                logger.warning(f"Socket error: {e}")
                time.sleep(min(self._backoff, 5))
                self._backoff = min(self._backoff * 2, 5)
                self._setup_socket()

        logger.error("Max retries exceeded")
        self._setup_socket()
        return None

    def parse_packet(self, packet):
        if packet is None:
            return None

        # Detect if this is from a Meshtastic gateway (lightweight)
        if "type" in packet and packet["type"] in ["occupancy", "event", "alert", "heartbeat"]:
            return {
                "source": "meshtastic_gateway",
                "data": packet
            }

        # Otherwise treat as normal CSI node data
        return {
            "source": "wifi_csi_node",
            "node": packet.get("node", "unknown"),
            "csi": packet.get("csi", [0.0]*32),
            "rssi": packet.get("rssi", -60),
            "timestamp": packet.get("timestamp")
        }

    def stream(self):
        while True:
            packet = self.read_packet()
            parsed = self.parse_packet(packet)
            if parsed:
                yield parsed


# ============================================================
# Standalone test mode
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="Standalone UDP CSI Ingestor Test Tool")
    parser.add_argument("--port", type=int, default=4210, help="UDP port to listen on (default: 4210)")
    parser.add_argument("--timeout", type=float, default=0.8, help="Socket timeout in seconds")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="[%(asctime)s] %(levelname)s: %(message)s"
    )

    print(f"\n=== CSI UDP Ingestor Test Mode ===")
    print(f"Listening on UDP port {args.port}")
    print("Waiting for packets from ESP32 nodes...\n")

    ingestor = CSIIngestor(udp_port=args.port)

    packet_count = 0

    try:
        while True:
            raw = ingestor.read_packet()
            if raw is None:
                continue

            parsed = ingestor.parse_packet(raw)

            packet_count += 1

            print(f"\n--- Packet #{packet_count} ---")
            print(f"Source : {parsed.get('source', 'unknown')}")

            if parsed.get("source") == "wifi_csi_node":
                print(f"Node   : {parsed.get('node')}")
                print(f"RSSI   : {parsed.get('rssi')} dBm")
                csi = parsed.get("csi", [])
                print(f"CSI    : {len(csi)} values | First 8: {csi[:8]}")
            else:
                print(f"Data   : {parsed.get('data')}")

            print(f"Raw    : {raw}")

    except KeyboardInterrupt:
        print("\n\nStopped by user.")


if __name__ == "__main__":
    main()
