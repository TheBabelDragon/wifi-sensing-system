import os
import json
import redis
import time
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger("sensing-command-listener")

class SensingCommandListener:
    """
    Robust listener for commands from aurora-swarm-btc.

    This class enables the sensing system to react to decisions made by the swarm.
    It is designed to be started as a background thread during normal operation.

    DISCLAIMER:
    Early stage. Commands are unauthenticated. Use in trusted environments only.
    """

    def __init__(self, redis_url: Optional[str] = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://redis:6379/0")
        self.r = redis.from_url(self.redis_url, decode_responses=True)
        self.pubsub = self.r.pubsub()
        self.handlers: Dict[str, Callable] = {}
        self.last_command_time = None
        self.command_count = 0

        # Default handlers
        self.register_handler("scale_down", self._handle_scale_down)
        self.register_handler("security_mode", self._handle_security_mode)
        self.register_handler("scale_up", self._handle_scale_up)

    def register_handler(self, command_type: str, handler: Callable[[Dict[str, Any]], None]):
        self.handlers[command_type] = handler

    def _handle_scale_down(self, command: Dict[str, Any]):
        factor = command.get("factor", 0.7)
        self._record_command("scale_down")
        logger.warning(f"[Command] scale_down received | factor={factor}")

    def _handle_security_mode(self, command: Dict[str, Any]):
        duration = command.get("duration_minutes", 10)
        self._record_command("security_mode")
        logger.warning(f"[Command] SECURITY_MODE activated | duration={duration}min")

    def _handle_scale_up(self, command: Dict[str, Any]):
        factor = command.get("factor", 1.2)
        self._record_command("scale_up")
        logger.info(f"[Command] scale_up received | factor={factor}")

    def _default_handler(self, command: Dict[str, Any]):
        self._record_command("unknown")
        logger.info(f"[Command] Unhandled command: {command}")

    def _record_command(self, cmd_type: str):
        self.last_command_time = time.time()
        self.command_count += 1

    def get_stats(self) -> Dict[str, Any]:
        """Return basic stats about received commands."""
        return {
            "total_commands_received": self.command_count,
            "last_command_time": self.last_command_time
        }

    def listen(self):
        self.pubsub.psubscribe("aurora:swarm:commands")
        logger.info("Command listener started")

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
                    logger.error(f"Error in handler for '{cmd_type}': {e}")

if __name__ == "__main__":
    listener = SensingCommandListener()
    listener.listen()