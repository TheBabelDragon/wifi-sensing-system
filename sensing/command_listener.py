import os
import json
import redis
import time
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger("sensing-command-listener")

class SensingCommandListener:
    """
    Listens for commands from aurora-swarm-btc.

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

        # Register some default example handlers
        self.register_handler("scale_down", self._handle_scale_down)
        self.register_handler("security_mode", self._handle_security_mode)
        self.register_handler("scale_up", self._handle_scale_up)

    def register_handler(self, command_type: str, handler: Callable[[Dict[str, Any]], None]):
        self.handlers[command_type] = handler

    def _handle_scale_down(self, command):
        factor = command.get("factor", 0.7)
        logger.warning(f"[Sensing] Received scale_down command. Factor: {factor}")
        # In a real system this could adjust CSI sampling rate, fusion sensitivity, etc.

    def _handle_security_mode(self, command):
        duration = command.get("duration_minutes", 10)
        logger.warning(f"[Sensing] SECURITY MODE activated for {duration} minutes")
        # Could trigger higher resolution tracking, more aggressive event detection, etc.

    def _handle_scale_up(self, command):
        factor = command.get("factor", 1.2)
        logger.info(f"[Sensing] Received scale_up command. Factor: {factor}")

    def _default_handler(self, command):
        logger.info(f"[Sensing] Unhandled command: {command}")

    def listen(self):
        self.pubsub.psubscribe("aurora:swarm:commands")
        logger.info("Listening for commands from aurora-swarm-btc...")

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
    listener.listen()