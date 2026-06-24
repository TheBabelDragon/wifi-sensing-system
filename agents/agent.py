from bridges.swarm_bridge import SwarmBridge
import time

class Agent:
    def __init__(self):
        self.swarm_bridge = SwarmBridge()
        self.last_action_time = {}
        self.cooldown_seconds = 30  # prevent spamming commands

        self.policy = {
            "OCCUPANCY_DETECTED": self._handle_occupancy,
            "ANOMALY_NEAR_RIGS": self._handle_anomaly,
            "RAPID_MOVEMENT": self._handle_rapid
        }

    def _can_act(self, action_type: str) -> bool:
        now = time.time()
        last = self.last_action_time.get(action_type, 0)
        if now - last > self.cooldown_seconds:
            self.last_action_time[action_type] = now
            return True
        return False

    def decide(self, state):
        events = state.get("events", [])
        tracks = state.get("tracks", [])

        # Priority: anomalies first
        if "ANOMALY_NEAR_RIGS" in events and self._can_act("anomaly"):
            return self._handle_anomaly(tracks)
        
        for event in events:
            if event in self.policy and self._can_act(event):
                return self.policy[event](tracks)
        
        return {"action": "monitor", "reason": "normal operation"}

    def execute(self, action):
        print(f"[Agent] Executing: {action}")
        if action.get("swarm_command"):
            print(f"[Agent] → Sending to aurora-swarm-btc: {action['swarm_command']}")
            # In production: call swarm scheduler API or publish command

    # Swarm-aware policies
    def _handle_occupancy(self, tracks):
        count = len(tracks)
        self.swarm_bridge.send_occupancy_event(room="mining_hall", count=count)
        if count > 2:
            return {
                "action": "reduce_power",
                "reason": "multiple humans detected near rigs",
                "swarm_command": {"type": "scale_down", "factor": 0.7, "duration_minutes": 15}
            }
        return {"action": "log_occupancy", "reason": "human presence noted"}

    def _handle_anomaly(self, tracks):
        self.swarm_bridge.send_anomaly_event(location="near_rigs", severity="high")
        return {
            "action": "security_alert",
            "reason": "anomaly near mining equipment",
            "swarm_command": {"type": "pause_mining", "reason": "security_alert", "duration_minutes": 5}
        }

    def _handle_rapid(self, tracks):
        self.swarm_bridge.send_anomaly_event(location="mining_area", severity="medium")
        return {"action": "increase_monitoring", "reason": "rapid movement detected"}