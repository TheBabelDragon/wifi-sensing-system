from aurora.adapter import AuroraAdapter
import time
import logging

logger = logging.getLogger("swarm-bridge")

class SwarmBridge:
    """
    Resilient bridge for sending context and heartbeats to aurora-swarm-btc.

    Now also supports publishing fused multi-node RoomState.
    """

    def __init__(self, redis_url: str = None):
        self.adapter = AuroraAdapter(redis_url=redis_url)
        self._backoff = 1

    def _safe_call(self, func, *args, **kwargs):
        for attempt in range(3):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                logger.warning(f"Redis operation failed (attempt {attempt+1}): {e}")
                time.sleep(min(self._backoff, 10))
                self._backoff = min(self._backoff * 2, 10)
        logger.error("Max retries exceeded for Redis operation")
        self._backoff = 1
        return None

    def send_heartbeat(self, extra: dict = None):
        payload = {
            "type": "HEARTBEAT",
            "timestamp": time.time(),
            "source": "wifi-csi-sensing"
        }
        if extra:
            payload.update(extra)
        self._safe_call(self.adapter.update_swarm_state, "sensing:heartbeat", payload)

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
        self._safe_call(self.adapter.publish_to_swarm, "sensing:context", payload)
        self._safe_call(self.adapter.update_swarm_state, "sensing:latest_context", payload)
        self.send_heartbeat()

    def send_room_state(self, room_state: dict):
        """
        Publish fused multi-node room state from NodeManager.
        This is the new richer payload for aurora-swarm-btc.
        """
        payload = {
            "type": "ROOM_STATE_UPDATE",
            "timestamp": time.time(),
            "source": "wifi-csi-sensing",
            "node_count": room_state.get("node_count", 0),
            "total_activity": room_state.get("total_activity", 0.0),
            "total_hot_zones": room_state.get("total_hot_zones", 0),
            "obstruction_probability": room_state.get("obstruction_probability", 0.0),
            "probability_field": room_state.get("probability_field", []),
            "nodes": room_state.get("nodes", {})
        }
        self._safe_call(self.adapter.publish_to_swarm, "sensing:room_state", payload)
        self._safe_call(self.adapter.update_swarm_state, "sensing:latest_room_state", payload)
        self.send_heartbeat()

    def send_occupancy_event(self, room: str, count: int, behavior: str = None):
        event = {
            "type": "OCCUPANCY_DETECTED",
            "room": room,
            "count": count,
            "behavior": behavior,
            "source": "wifi-csi-sensing"
        }
        self._safe_call(self.adapter.publish_to_swarm, "sensing:events", event)
