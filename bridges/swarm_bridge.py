from aurora.adapter import AuroraAdapter
import time
import logging

logger = logging.getLogger("swarm-bridge")

class SwarmBridge:
    def __init__(self, redis_url: str = None):
        self.adapter = AuroraAdapter(redis_url=redis_url)

    def send_heartbeat(self, extra: dict = None):
        """Send heartbeat so the swarm knows the sensing system is alive."""
        payload = {
            "type": "HEARTBEAT",
            "timestamp": time.time(),
            "source": "wifi-csi-sensing"
        }
        if extra:
            payload.update(extra)
        self.adapter.update_swarm_state("sensing:heartbeat", payload)

    def send_full_context(self, tracks, events, behaviors, memory_profile):
        payload = {
            "type": "FULL_CONTEXT_UPDATE",
            "timestamp": time.time(),
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
        self.send_heartbeat()  # Heartbeat piggybacked on rich updates