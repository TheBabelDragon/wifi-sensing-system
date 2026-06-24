import os
import json
import redis
import time
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger("sensing-command-listener")

class SensingCommandListener:
    """
    Listens for structured commands from aurora-swarm-btc via Redis.

    This enables true bidirectional communication:
    - Sensing system sends rich context + heartbeats to the swarm
    - Swarm can send commands back (scale_down, security_mode, scale_up, etc.)

    DISCLAIMER:
    This is an early-stage bidirectional control channel.
    Commands are currently unauthenticated and minimally validated.
    Use only in trusted/internal environments.
    """

    def __init__(self, redis_url: Optional[str] = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://redis:6379/0")
        self.r = redis.from_url(self.redis_url, decode_responses=True)
        self.pubsub = self.r.pubsub()
        self.handlers: Dict[str, Callable] = {}

        # Register default handlers for common swarm commands
        self.register_handler("scale_down", self._handle_scale_down)
        self.register_handler("security_mode", self._handle_security_mode)
        self.register_handler("scale_up", self._handle_scale_up)

    def register_handler(self, command_type: str, handler: Callable[[Dict[str, Any]], None]):
        """Register a handler function for a specific command type."""
        self.handlers[command_type] = handler

    def _handle_scale_down(self, command: Dict[str, Any]):
        factor = command.get("factor", 0.7)
        logger.warning(f"[Command] scale_down received | factor={factor}")
        # Placeholder for future: could reduce CSI sampling rate or fusion sensitivity

    def _handle_security_mode(self, command: Dict[str, Any]):
        duration = command.get("duration_minutes", 10)
        logger.warning(f"[Command] SECURITY_MODE activated | duration={duration}min")
        # Placeholder for future: could increase tracking resolution + event sensitivity

    def _handle_scale_up(self, command: Dict[str, Any]):
        factor = command.get("factor", 1.2)
        logger.info(f"[Command] scale_up received | factor={factor}")

    def _default_handler(self, command: Dict[str, Any]):
        logger.info(f"[Command] Unhandled command type: {command}")

    def listen(self):
        """Start listening for commands on the Redis pub/sub channel."""
        self.pubsub.psubscribe("aurora:swarm:commands")
        logger.info("Bidirectional command listener started (listening on aurora:swarm:commands)")

        for message in self.pubsub.listen():
            if message['type'] == 'pmessage':
                try:
                    command = json.loads(message['data'])
                except Exception:
                    command = {"raw": message['data']}

                cmd_type = command.get("type") or command.get("action", "unknown")
                handler = self.handlers.get(cmd_type, self._default_handler)

                try:
                    handler(command)
                except Exception as e:
                    logger.error(f"Error executing handler for {cmd_type}: {e}")

if __name__ == "__main__":
    listener = SensingCommandListener()
    listener.listen()