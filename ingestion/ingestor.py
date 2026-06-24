class CSIIngestor:
    def parse_csi(self, packet):
        return {
            "node": packet.get("node", "unknown"),
            "csi": packet.get("csi", [0.0]*32),
            "rssi": packet.get("rssi", -60)
        }

    def stream(self):
        # Placeholder stream
        for i in range(5):
            yield self.parse_csi({"node": f"esp32_{i}", "csi": [0.5]*32, "rssi": -50})