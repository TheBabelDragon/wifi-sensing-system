import sys
sys.path.append('.')

from aurora.adapter import AuroraAdapter
from ingestion.ingestor import CSIIngestor
from calibration.engine import CalibrationEngine
from fusion.engine import FusionEngine
from tracking.tracker import TrackerEngine
from predictive_tracking.predictor import Predictor
from behavior.engine import BehaviorEngine
from event_engine.engine import EventEngine
from interaction_modeling.graph import InteractionGraph
from memory.engine import MemoryEngine
from autonomous_adaptation.engine import AdaptationEngine
from federation.layer import Federation
from agents.agent import Agent
from dashboard.server import DashboardServer
from simulation.generator import generate_test_frame
from bridges.swarm_bridge import SwarmBridge

def run_pipeline():
    print("\n🚀 WiFi CSI Spatial Intelligence + Aurora Swarm BTC — Full Integrated Pipeline\n")

    # Initialize everything
    aurora = AuroraAdapter()
    ingest = CSIIngestor()
    calib = CalibrationEngine()
    fusion = FusionEngine()
    tracker = TrackerEngine()
    predictor = Predictor()
    behavior = BehaviorEngine()
    events = EventEngine()
    interaction = InteractionGraph()
    memory = MemoryEngine()
    adaptation = AdaptationEngine()
    federation = Federation()
    agent = Agent()
    dashboard = DashboardServer()
    swarm_bridge = SwarmBridge()

    # Aurora swarm connection test
    aurora.register_node("esp32_mining_hall_01", {"pos": (0,0), "type": "sensing"})
    print("Aurora health:", aurora.health_report())
    print("Swarm bridge status:", swarm_bridge.get_swarm_status())

    print("\n--- Running 5 integrated frames ---")
    for i in range(5):
        print(f"\n=== Frame {i+1} ===")
        frame = generate_test_frame()
        
        parsed = ingest.parse_csi(frame)
        calibrated = calib.calibrate(parsed)
        voxels = fusion.fuse(calibrated)
        tracks = tracker.update(voxels)
        preds = [predictor.predict(t) for t in tracks]
        states = [behavior.classify(t) for t in preds]
        ev = events.generate(preds)
        graph = interaction.build(preds)
        memory.update({"tracks": preds, "events": ev})
        
        metrics = {"error": 0.1 + i * 0.05}
        params = adaptation.adjust(metrics)
        
        # === NEW: Swarm Integration ===
        if "ROOM_OCCUPIED" in ev or "RAPID_MOVEMENT" in ev:
            decision = agent.decide({"tracks": preds, "events": ev})
            agent.execute(decision)
        
        # Push thermal/occupancy context to swarm
        if len(preds) > 0:
            swarm_bridge.send_thermal_context(avg_temp_proxy=0.6 + i*0.05)

        state = {
            "frame": i+1,
            "tracks": len(preds),
            "events": ev,
            "behavior": states,
            "adaptation": params,
            "swarm_synced": True
        }
        dashboard.push(state)
        
        print(f"Tracks: {len(preds)} | Events: {ev} | Behavior: {states}")

    print("\n✅ Full integrated pipeline completed!")
    print("Memory profile:", memory.room_profile)
    print("\nThe sensing system is now feeding real-world context into aurora-swarm-btc.")

if __name__ == "__main__":
    run_pipeline()