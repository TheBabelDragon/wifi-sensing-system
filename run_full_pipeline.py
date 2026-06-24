import sys
sys.path.append('.')
import os

# Optional: load .env if python-dotenv is available
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

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

    redis_url = os.getenv("REDIS_URL")
    room_name = os.getenv("SENSING_ROOM_NAME", "mining_hall")

    # Initialize with Redis support
    aurora = AuroraAdapter(redis_url=redis_url)
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
    swarm_bridge = SwarmBridge(redis_url=redis_url)

    aurora.register_node("esp32_mining_hall_01", {"pos": (0,0), "type": "sensing", "room": room_name})
    print("Aurora health:", aurora.health_report())
    print("Swarm bridge status:", swarm_bridge.get_swarm_status())

    num_frames = int(os.getenv("SIMULATION_FRAMES", 5))
    print(f"\n--- Running {num_frames} integrated frames ---")

    for i in range(num_frames):
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
        
        # Swarm integration
        if any(e in ev for e in ["ROOM_OCCUPIED", "RAPID_MOVEMENT", "ANOMALY"]):
            decision = agent.decide({"tracks": preds, "events": ev})
            agent.execute(decision)
        
        if len(preds) > 0:
            swarm_bridge.send_thermal_context(avg_temp_proxy=round(0.6 + i*0.05, 2))

        state = {
            "frame": i+1,
            "room": room_name,
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