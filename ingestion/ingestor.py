import socket
import json

class CSIIngestor:
    def __init__(self, udp_port: int = 4210):
        self.udp_port = udp_port
        self.sock = None
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(("0.0.0.0", self.udp_port))
            self.sock.settimeout(1.0)
            print(f"[CSIIngestor] Listening for ESP32 CSI on UDP port {self.udp_port}")
        except Exception as e:
            print(f"[CSIIngestor] UDP setup failed (simulation mode): {e}")

    def read_packet(self):
        if self.sock:
            try:
                data, addr = self.sock.recvfrom(1024)
                return json.loads(data.decode())
            except socket.timeout:
                return None
            except Exception:
                return None
        return None

    def parse_csi(self, packet):
        if packet is None:
            # Fallback simulation
            return {
                "node": "simulated_esp32",
                "csi": [0.5] * 32,
                "rssi": -55,
                "timestamp": "sim"
            }
        return {
            "node": packet.get("node", "unknown"),
            "csi": packet.get("csi", [0.0]*32),
            "rssi": packet.get("rssi", -60),
            "timestamp": packet.get("timestamp")
        }

    def stream(self):
        while True:
            packet = self.read_packet()
            if packet:
                yield self.parse_csi(packet)
            else:
                # Yield simulated data if no real packet
                yield self.parse_csi(None)