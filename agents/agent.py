from bridges.swarm_bridge import SwarmBridge

class Agent:
    def __init__(self):
        self.swarm_bridge = SwarmBridge()
        self.policy = {
            "OCCUPANCY_DETECTED": self._handle_occupancy,
            "ANOMALY_NEAR_RIGS": self._handle_anomaly,
            "RAPID_MOVEMENT": self._handle_rapid
        }

    def decide(self, state):
        events = state.get("events", [])
        tracks = state.get("tracks", [])
        
        for event in events:
            if event in self.policy:
                action = self.policy[event](tracks)
                return action
        
        return {"action": "monitor", "reason": "normal operation"}

    def execute(self, action):
        print(f"[Agent] Executing: {action}")
        if action.get("swarm_command"):
            # In real deployment this would call into swarm scheduler
            print(f"[Agent] → Sending command to aurora-swarm-btc: {action['swarm_command']}")

    # === Swarm-aware policies ===
    def _handle_occupancy(self, tracks):
        count = len(tracks)
        self.swarm_bridge.send_occupancy_event(room="mining_hall", count=count, behavior="present")
        if count > 2:
            return {
                "action": "reduce_power",
                "reason": "multiple humans detected",
                "swarm_command": {"type": "scale_down", "factor": 0.7}
            }
        return {"action": "log_occupancy", "reason": "human presence noted"}

    def _handle_anomaly(self, tracks):
        self.swarm_bridge.send_anomaly_event(location="near_rigs", severity="high")
        return {
            "action": "alert_and_freeze",
            "reason": "anomaly near mining equipment",
            "swarm_command": {"type": "pause_mining", "reason": "security_alert"}
        }

    def _handle_rapid(self, tracks):
        self.swarm_bridge.send_anomaly_event(location="mining_area", severity="medium")
        return {"action": "increase_monitoring", "reason": "rapid movement detected"}