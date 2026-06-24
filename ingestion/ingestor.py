import socket
import json
import logging
import time

logger = logging.getLogger("csi-ingestor")

class CSIIngestor:
    """
    Resilient UDP ingestor for ESP32 CSI data.

    Features:
    - Automatic socket reconnection
    - Retry logic with backoff
    - Graceful fallback to simulation
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
            logger.info(f"Listening for ESP32 CSI on UDP port {self.udp_port}")
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
                logger.warning(f"Socket error (attempt {attempt+1}): {e}")
                time.sleep(min(self._backoff, 5))
                self._backoff = min(self._backoff * 2, 5)
                self._setup_socket()

        logger.error("Max retries exceeded on UDP read")
        self._setup_socket()
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
