import os
import json
try:
    import redis
except ImportError:
    redis = None

class AuroraAdapter:
    def __init__(self, redis_url: str = None):
        self.nodes = {}
        self.sync_state = None
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://localhost:6379/0")
        self.bus = None
        if redis:
            try:
                self.bus = redis.from_url(self.redis_url, socket_connect_timeout=5, socket_timeout=5)
            except Exception:
                self.bus = None

    def register_node(self, node_id, metadata):
        self.nodes[node_id] = metadata

    def get_synchronized_frame(self):
        return {
            "nodes": self.nodes,
            "timestamp": "synced"
        }

    def health_report(self):
        return {"status": "ok", "nodes": len(self.nodes)}

    # === NEW: Swarm Integration Methods ===
    def publish_to_swarm(self, channel: str, message: dict):
        """Publish sensing events to aurora-swarm-btc control bus"""
        if not self.bus:
            print(f"[AuroraAdapter] (simulated) Publishing to aurora:{channel}: {message}")
            return
        try:
            key = f"aurora:{channel}"
            if isinstance(message, (dict, list)):
                val = json.dumps(message)
            else:
                val = str(message)
            self.bus.publish(key, val)
            print(f"[AuroraAdapter] Published to aurora:{channel}")
        except Exception as e:
            print(f"[AuroraAdapter] Publish error: {e}")

    def update_swarm_state(self, key: str, value):
        """Update shared state in the swarm bus"""
        if not self.bus:
            print(f"[AuroraAdapter] (simulated) Set aurora:{key} = {value}")
            return
        try:
            full_key = f"aurora:{key}"
            val = json.dumps(value) if isinstance(value, (dict, list)) else str(value)
            self.bus.set(full_key, val)
            print(f"[AuroraAdapter] Updated swarm state: {key}")
        except Exception as e:
            print(f"[AuroraAdapter] State update error: {e}")