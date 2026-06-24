from aurora.adapter import AuroraAdapter
import json

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

    def send_full_context(self, tracks: list, events: list, behaviors: list, memory_profile: dict):
        """Send rich structured context to the swarm (secure channel)."""
        payload = {
            "type": "FULL_CONTEXT_UPDATE",
            "timestamp": __import__('time').time(),
            "tracks": tracks,
            "events": events,
            "behaviors": behaviors,
            "memory_summary": {
                "avg_occupancy": memory_profile.get("avg_occupancy"),
                "total_events": memory_profile.get("total_occupancy_events")
            },
            "source": "wifi-csi-sensing"
        }
        self.adapter.publish_to_swarm("sensing:context", payload)
        self.adapter.update_swarm_state("sensing:latest_context", payload)

    def send_thermal_context(self, avg_temp_proxy: float):
        self.adapter.update_swarm_state("sensing:thermal_context", {
            "avg_temp_proxy": avg_temp_proxy,
            "timestamp": __import__('time').time()
        })