import os
import json
import redis
import time
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger("sensing-command-listener")

class SensingCommandListener:
    """
    Listens for commands from aurora-swarm-btc and reacts.

    WARNING: This is an early bidirectional control channel.
    Commands are not authenticated or validated beyond basic structure.
    Use with caution in production environments.
    """

    def __init__(self, redis_url: Optional[str] = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://redis:6379/0")
        self.r = redis.from_url(self.redis_url, decode_responses=True)
        self.pubsub = self.r.pubsub()
        self.handlers: Dict[str, Callable] = {}

    def register_handler(self, command_type: str, handler: Callable[[Dict[str, Any]], None]):
        self.handlers[command_type] = handler
        logger.info(f"Registered handler for command type: {command_type}")

    def _default_handler(self, command: Dict[str, Any]):
        logger.info(f"[Sensing] Received command (no handler): {command}")

    def listen(self):
        self.pubsub.psubscribe("aurora:swarm:commands")
        logger.info("Listening for commands from aurora-swarm-btc on aurora:swarm:commands...")

        for message in self.pubsub.listen():
            if message['type'] == 'pmessage':
                try:
                    command = json.loads(message['data'])
                except:
                    command = {"raw": message['data']}

                cmd_type = command.get("type") or command.get("action", "unknown")

                handler = self.handlers.get(cmd_type, self._default_handler)
                try:
                    handler(command)
                except Exception as e:
                    logger.error(f"Error handling command {cmd_type}: {e}")

if __name__ == "__main__":
    listener = SensingCommandListener()

    def example_handler(cmd):
        logger.info(f"Example handler received: {cmd}")

    listener.register_handler("scale_down", example_handler)
    listener.register_handler("security_mode", example_handler)
    listener.listen()