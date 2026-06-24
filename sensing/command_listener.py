import os
import json
import redis
import time
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger("sensing-command-listener")

class SensingCommandListener:
    """
    Robust listener for commands coming from aurora-swarm-btc.

    Features:
    - Registers handlers for specific command types
    - Graceful error handling per command
    - Clear logging
    - Default handlers for common swarm commands

    DISCLAIMER:
    This is an early bidirectional control channel.
    Commands are currently unauthenticated.
    Use only in trusted environments.
    """

    def __init__(self, redis_url: Optional[str] = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://redis:6379/0")
        self.r = redis.from_url(self.redis_url, decode_responses=True)
        self.pubsub = self.r.pubsub()
        self.handlers: Dict[str, Callable] = {}

        # Default handlers
        self.register_handler("scale_down", self._handle_scale_down)
        self.register_handler("security_mode", self._handle_security_mode)
        self.register_handler("scale_up", self._handle_scale_up)

    def register_handler(self, command_type: str, handler: Callable[[Dict[str, Any]], None]):
        """Register a handler for a command type."""
        self.handlers[command_type] = handler

    def _handle_scale_down(self, command: Dict[str, Any]):
        factor = command.get("factor", 0.7)
        logger.warning(f"[Command Received] scale_down | factor={factor}")

    def _handle_security_mode(self, command: Dict[str, Any]):
        duration = command.get("duration_minutes", 10)
        logger.warning(f"[Command Received] SECURITY_MODE | duration={duration}min")

    def _handle_scale_up(self, command: Dict[str, Any]):
        factor = command.get("factor", 1.2)
        logger.info(f"[Command Received] scale_up | factor={factor}")

    def _default_handler(self, command: Dict[str, Any]):
        logger.info(f"[Command Received] Unhandled: {command}")

    def listen(self):
        """Start listening for commands."""
        self.pubsub.psubscribe("aurora:swarm:commands")
        logger.info("Command listener active on aurora:swarm:commands")

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
                    logger.error(f"Handler error for '{cmd_type}': {e}")

if __name__ == "__main__":
    listener = SensingCommandListener()
    listener.listen()