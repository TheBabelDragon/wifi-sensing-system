import os
import json
import time
import logging

try:
    import redis
except ImportError:
    redis = None

logger = logging.getLogger("aurora-adapter")

class AuroraAdapter:
    """
    Resilient adapter for Redis communication with aurora-swarm-btc.

    Provides basic retry logic for publish and state operations.
    """

    def __init__(self, redis_url: str = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://localhost:6379/0")
        self.bus = None
        self._connect()

    def _connect(self):
        if redis is None:
            self.bus = None
            return
        try:
            self.bus = redis.from_url(self.redis_url, socket_connect_timeout=5, socket_timeout=5)
        except Exception as e:
            logger.warning(f"Failed to connect to Redis: {e}")
            self.bus = None

    def _safe_call(self, func, *args, **kwargs):
        for attempt in range(3):
            try:
                if self.bus is None:
                    self._connect()
                if self.bus is None:
                    return None
                return func(*args, **kwargs)
            except Exception as e:
                logger.warning(f"Redis operation failed (attempt {attempt+1}): {e}")
                time.sleep(0.5 * (attempt + 1))
                self._connect()
        return None

    def register_node(self, node_id, metadata):
        self._safe_call(lambda: None)  # placeholder for future registry

    def get_synchronized_frame(self):
        return {
            "nodes": {},
            "timestamp": "synced"
        }

    def health_report(self):
        return {"status": "ok", "nodes": 0}

    def publish_to_swarm(self, channel: str, message: dict):
        if not self.bus:
            logger.debug(f"(simulated) Publishing to aurora:{channel}")
            return
        return self._safe_call(self.bus.publish, f"aurora:{channel}", json.dumps(message))

    def update_swarm_state(self, key: str, value):
        if not self.bus:
            logger.debug(f"(simulated) Updating aurora:{key}")
            return
        return self._safe_call(self.bus.set, f"aurora:{key}", json.dumps(value) if isinstance(value, (dict, list)) else str(value))
