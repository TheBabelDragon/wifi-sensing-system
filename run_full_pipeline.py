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

def run_pipeline():
    print("\n🚀 Starting WiFi Sensing System v1.1.0 - Full Operable Pipeline\n")

    # Initialize all modules
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

    # Register some nodes via Aurora
    aurora.register_node("esp32_1", {"pos": (0,0)})
    aurora.register_node("esp32_2", {"pos": (5,0)})
    print("Aurora health:", aurora.health_report())

    # Simulate frames
    for i in range(5):
        print(f"\n--- Frame {i+1} ---")
        frame = generate_test_frame()
        
        # Ingest & parse
        parsed = ingest.parse_csi(frame)
        
        # Calibrate
        calibrated = calib.calibrate(parsed)
        
        # Fuse to voxel field
        voxels = fusion.fuse(calibrated)
        
        # Track
        tracks = tracker.update(voxels)
        
        # Predict
        preds = [predictor.predict(t) for t in tracks]
        
        # Behavior
        states = [behavior.classify(t) for t in preds]
        
        # Events
        ev = events.generate(preds)
        
        # Interaction graph
        graph = interaction.build(preds)
        
        # Memory
        memory.update({"tracks": preds, "events": ev})
        
        # Adaptation
        metrics = {"error": 0.1 + i*0.05}
        params = adaptation.adjust(metrics)
        
        # Federation (example)
        room_embed = federation.encode_room(memory.room_profile)
        
        # Agent decision
        decision = agent.decide({"tracks": preds, "events": ev})
        agent.execute(decision)
        
        # Dashboard
        state = {
            "frame": i+1,
            "voxels": voxels,
            "tracks": preds,
            "events": ev,
            "behavior": states,
            "adaptation_params": params
        }
        dashboard.push(state)
        
        print(f"Tracks: {len(preds)} | Events: {ev} | Behavior: {states}")

    print("\n✅ Pipeline completed successfully!")
    print("Memory snapshot:", memory.room_profile)

if __name__ == "__main__":
    run_pipeline()