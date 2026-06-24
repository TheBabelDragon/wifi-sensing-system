from bridges.swarm_bridge import SwarmBridge
from agents.agent import Agent

print("=== Integrated Sensing + Swarm Demo ===")

bridge = SwarmBridge()
agent = Agent()

# Simulate some events
bridge.send_occupancy_event("mining_hall", count=3, behavior="walking")
bridge.send_anomaly_event("near_rig_03", severity="high")

# Agent reacts
state = {"events": ["OCCUPANCY_DETECTED", "ANOMALY_NEAR_RIGS"], "tracks": [{"id":1}, {"id":2}, {"id":3}]}
decision = agent.decide(state)
agent.execute(decision)

print("\nDemo complete. Events flowed into aurora-swarm-btc control bus.")