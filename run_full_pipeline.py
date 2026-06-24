import sys
sys.path.append('.')
import os
import time
import logging

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

from config import config

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
from visualization.text_viz import render_voxel_field

# Setup logging
logging.basicConfig(
    level=logging.INFO if config.VERBOSE else logging.WARNING,
    format='[%(asctime)s] %(levelname)s - %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger("csi-system")

def run_pipeline():
    logger.info("="*65)
    logger.info("  WiFi CSI Spatial Intelligence System v1.1.0 - Production Demo")
    logger.info("="*65)

    # Initialize with config
    aurora = AuroraAdapter(redis_url=config.REDIS_URL)
    ingest = CSIIngestor(udp_port=config.UDP_PORT)
    calib = CalibrationEngine()
    fusion = FusionEngine(grid_size=24)
    tracker = TrackerEngine(max_distance=config.MAX_TRACK_DISTANCE)
    predictor = Predictor()
    behavior = BehaviorEngine()
    events = EventEngine()
    interaction = InteractionGraph()
    memory = MemoryEngine()
    adaptation = AdaptationEngine()
    federation = Federation()
    agent = Agent()
    dashboard = DashboardServer()
    swarm_bridge = SwarmBridge(redis_url=config.REDIS_URL)

    aurora.register_node(f"esp32_{config.ROOM_NAME}_01", {"pos": (2,2), "type": "sensing"})
    logger.info(f"Initialized. Aurora health: {aurora.health_report()}")

    for frame_idx in range(1, config.SIMULATION_FRAMES + 1):
        logger.info(f"\n--- Frame {frame_idx} ---")

        raw_frame = generate_test_frame(node_id=f"esp32_{config.ROOM_NAME}_01")
        parsed = ingest.parse_csi(raw_frame)
        calibrated = calib.calibrate(parsed)

        voxel_field = fusion.fuse(calibrated)
        tracks = tracker.update(voxel_field)
        predictions = [predictor.predict(t) for t in tracks]
        behaviors = [behavior.classify(p) for p in predictions]
        evs = events.generate(predictions)

        memory.update({"tracks": predictions, "events": evs})
        params = adaptation.adjust({"error": round(0.08 + frame_idx * 0.035, 2)})

        if evs:
            decision = agent.decide({"tracks": predictions, "events": evs})
            agent.execute(decision)
            if decision.get("swarm_command"):
                swarm_bridge.send_occupancy_event(config.ROOM_NAME, len(predictions))

        state = {
            "frame": frame_idx,
            "room": config.ROOM_NAME,
            "tracks": len(predictions),
            "events": evs,
            "behaviors": behaviors
        }
        dashboard.push(state)

        # Clean logging
        logger.info(f"Tracks: {len(predictions)} | Events: {evs} | Behaviors: {behaviors}")
        render_voxel_field(voxel_field, predictions)

        time.sleep(config.DEMO_SLEEP)

    logger.info("\nDemonstration complete.")
    logger.info(f"Final memory profile: {memory.room_profile}")

if __name__ == "__main__":
    run_pipeline()