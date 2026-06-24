from aurora.adapter import AuroraAdapter
import time
import logging

logger = logging.getLogger("swarm-bridge")

class SwarmBridge:
    def __init__(self, redis_url: str = None, max_retries: int = 3):
        self.adapter = AuroraAdapter(redis_url=redis_url)
        self.max_retries = max_retries

    def _safe_publish(self, method, *args, **kwargs):
        for attempt in range(self.max_retries):
            try:
                return method(*args, **kwargs)
            except Exception as e:
                logger.warning(f"Redis operation failed (attempt {attempt+1}): {e}")
                time.sleep(0.2 * (attempt + 1))
        logger.error("Max retries exceeded for Redis operation")
        return None

    def send_occupancy_event(self, room: str, count: int, behavior: str = None):
        event = {
            "type": "OCCUPANCY_DETECTED",
            "room": room,
            "count": count,
            "behavior": behavior,
            "source": "wifi-csi-sensing"
        }
        self._safe_publish(self.adapter.publish_to_swarm, "sensing:events", event)
        self._safe_publish(self.adapter.update_swarm_state, "sensing:last_occupancy", event)

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
        self._safe_publish(self.adapter.publish_to_swarm, "sensing:context", payload)
        self._safe_publish(self.adapter.update_swarm_state, "sensing:latest_context", payload)