import socket
import json
import logging

logger = logging.getLogger("csi-ingestor")

class CSIIngestor:
    def __init__(self, udp_port: int = 4210):
        self.udp_port = udp_port
        self.sock = None
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(("0.0.0.0", self.udp_port))
            self.sock.settimeout(0.8)
            logger.info(f"Listening for ESP32 CSI on UDP port {self.udp_port}")
        except Exception as e:
            logger.warning(f"UDP listener failed to start: {e} (running in simulation mode)")

    def read_packet(self):
        if self.sock:
            try:
                data, addr = self.sock.recvfrom(2048)
                return json.loads(data.decode())
            except socket.timeout:
                return None
            except Exception as e:
                logger.debug(f"Packet receive error: {e}")
                return None
        return None

    def parse_csi(self, packet):
        if packet is None:
            return {
                "node": "simulated",
                "csi": [0.4] * 32,
                "rssi": -58,
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
            yield self.parse_csi(packet)