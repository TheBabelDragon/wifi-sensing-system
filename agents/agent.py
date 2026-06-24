from bridges.swarm_bridge import SwarmBridge
import time
import logging

logger = logging.getLogger("csi-agent")

class Agent:
    def __init__(self, cooldown: int = 25):
        self.swarm_bridge = SwarmBridge()
        self.last_action_time = {}
        self.cooldown = cooldown

    def _can_act(self, key: str) -> bool:
        now = time.time()
        if now - self.last_action_time.get(key, 0) > self.cooldown:
            self.last_action_time[key] = now
            return True
        return False

    def decide(self, state):
        events = state.get("events", [])
        tracks = state.get("tracks", [])

        if "ANOMALY_NEAR_RIGS" in events and self._can_act("anomaly"):
            return self._handle_anomaly(tracks)

        for event in events:
            if self._can_act(event):
                if event == "OCCUPANCY_DETECTED":
                    return self._handle_occupancy(tracks)
                if event == "RAPID_MOVEMENT":
                    return self._handle_rapid(tracks)

        return {"action": "monitor", "reason": "normal"}

    def execute(self, action):
        logger.info(f"Agent decision: {action.get('action')} | {action.get('reason', '')}")
        if action.get("swarm_command"):
            logger.info(f"→ Sending to swarm: {action['swarm_command']}")

    def _handle_occupancy(self, tracks):
        count = len(tracks)
        self.swarm_bridge.send_occupancy_event("mining_hall", count)
        if count > 2:
            return {"action": "scale_down", "reason": "multiple humans", "swarm_command": {"type": "scale_down", "factor": 0.7}}
        return {"action": "log", "reason": "occupancy noted"}

    def _handle_anomaly(self, tracks):
        self.swarm_bridge.send_anomaly_event("near_rigs", "high")
        return {"action": "security_alert", "reason": "anomaly detected", "swarm_command": {"type": "pause_mining"}}

    def _handle_rapid(self, tracks):
        return {"action": "monitor", "reason": "rapid movement"}