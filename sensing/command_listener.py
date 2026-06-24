import os
import json
import redis
import time
import logging
from typing import Callable, Optional, Dict, Any

try:
    from observability.metrics import get_metrics
    HAS_METRICS = True
except ImportError:
    HAS_METRICS = False

logger = logging.getLogger("sensing-command-listener")

VALID_COMMAND_TYPES = {"scale_down", "security_mode", "scale_up"}

class SensingCommandListener:
    """
    Resilient command listener with metrics.

    See INTEGRATION_CONTRACT.md for message format.
    """

    def __init__(self, redis_url: Optional[str] = None):
        self.redis_url = redis_url or os.getenv("REDIS_URL", "redis://redis:6379/0")
        self.r = redis.from_url(self.redis_url, decode_responses=True)
        self.pubsub = self.r.pubsub()
        self.handlers: Dict[str, Callable] = {}
        self.stats = {"total": 0, "by_type": {}}
        self.last_command_time = None

        if HAS_METRICS:
            self.metrics = get_metrics()

        self.register_handler("scale_down", self._handle_scale_down)
        self.register_handler("security_mode", self._handle_security_mode)
        self.register_handler("scale_up", self._handle_scale_up)

    def register_handler(self, command_type: str, handler: Callable):
        self.handlers[command_type] = handler

    def _validate_command(self, command: Dict[str, Any]) -> bool:
        cmd_type = command.get("type") or command.get("action")
        return cmd_type in VALID_COMMAND_TYPES

    def _record_stats(self, cmd_type: str, success: bool = True):
        self.stats["total"] += 1
        self.stats["by_type"][cmd_type] = self.stats["by_type"].get(cmd_type, 0) + 1
        self.last_command_time = time.time()

        if HAS_METRICS:
            self.metrics.record_command(cmd_type, success)

    def _handle_scale_down(self, command):
        factor = command.get("factor", 0.7)
        self._record_stats("scale_down")
        logger.warning(f"[Command] scale_down | factor={factor}")

    def _handle_security_mode(self, command):
        duration = command.get("duration_minutes", 10)
        self._record_stats("security_mode")
        logger.warning(f"[Command] SECURITY_MODE | duration={duration}min")

    def _handle_scale_up(self, command):
        factor = command.get("factor", 1.2)
        self._record_stats("scale_up")
        logger.info(f"[Command] scale_up | factor={factor}")

    def _default_handler(self, command):
        self._record_stats("unknown", success=False)
        logger.info(f"[Command] Unhandled: {command}")

    def get_stats(self):
        return {
            "total_commands": self.stats["total"],
            "by_type": self.stats["by_type"].copy(),
            "last_command_time": self.last_command_time
        }

    def listen(self):
        self.pubsub.psubscribe("aurora:swarm:commands")
        logger.info("Command listener active")

        for message in self.pubsub.listen():
            if message['type'] == 'pmessage':
                try:
                    command = json.loads(message['data'])
                except Exception:
                    command = {"raw": message['data']}

                cmd_type = command.get("type") or command.get("action", "unknown")

                if not self._validate_command(command) and cmd_type != "unknown":
                    logger.warning(f"Invalid command type: {cmd_type}")
                    self._record_stats(cmd_type, success=False)
                    return

                handler = self.handlers.get(cmd_type, self._default_handler)
                try:
                    handler(command)
                except Exception as e:
                    logger.error(f"Handler error for {cmd_type}: {e}")
                    self._record_stats(cmd_type, success=False)

if __name__ == "__main__":
    listener = SensingCommandListener()
    listener.listen()