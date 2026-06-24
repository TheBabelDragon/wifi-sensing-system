from aurora.adapter import AuroraAdapter

class SwarmBridge:
    def __init__(self, redis_url: str = None):
        self.adapter = AuroraAdapter(redis_url=redis_url)

    def send_occupancy_event(self, room: str, count: int, behavior: str = None):
        event = {
            "type": "OCCUPANCY_DETECTED",
            "room": room,
            "count": count,
            "behavior": behavior,
            "source": "wifi-csi-sensing"
        }
        self.adapter.publish_to_swarm("sensing:events", event)
        self.adapter.update_swarm_state("sensing:last_occupancy", event)

    def send_anomaly_event(self, location: str, severity: str = "medium"):
        event = {
            "type": "ANOMALY_NEAR_RIGS",
            "location": location,
            "severity": severity,
            "source": "wifi-csi-sensing"
        }
        self.adapter.publish_to_swarm("sensing:alerts", event)

    def send_thermal_context(self, avg_temp_proxy: float):
        """Proxy for heat signature / occupancy heat"""
        self.adapter.update_swarm_state("sensing:thermal_context", {
            "avg_temp_proxy": avg_temp_proxy,
            "timestamp": "now"
        })

    def get_swarm_status(self):
        # Placeholder - in real deployment would read from bus
        return {"status": "connected", "mood": "They yearn for the mines"}