import os
import json
try:
    import redis
except ImportError:
    redis = None

class SwarmEventListener:
    """Example listener that the aurora-swarm-btc side can use to react to sensing events."""
    def __init__(self, redis_url: str = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://localhost:6379/0")
        self.r = None
        if redis:
            try:
                self.r = redis.from_url(self.redis_url)
            except Exception:
                self.r = None

    def listen_for_sensing_events(self, callback=None):
        if not self.r:
            print("[SwarmListener] No Redis — running in demo mode")
            return

        pubsub = self.r.pubsub()
        pubsub.psubscribe("aurora:sensing:*")
        print("[SwarmListener] Listening on aurora:sensing:* channels...")

        for message in pubsub.listen():
            if message['type'] == 'pmessage':
                channel = message['channel'].decode()
                try:
                    data = json.loads(message['data'].decode())
                except:
                    data = message['data'].decode()

                print(f"[SwarmListener] Received on {channel}: {data}")

                if callback:
                    callback(channel, data)
                else:
                    self._default_handler(channel, data)

    def _default_handler(self, channel: str, data):
        if "OCCUPANCY_DETECTED" in str(data):
            print("[Swarm] → Occupancy detected. Considering power scaling...")
        elif "ANOMALY" in str(data):
            print("[Swarm] → Anomaly alert! Triggering security protocol.")

if __name__ == "__main__":
    listener = SwarmEventListener()
    listener.listen_for_sensing_events()