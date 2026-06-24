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

import threading
try:
    from dashboard_web.app import app as web_app
    import uvicorn
    HAS_WEB = True
except ImportError:
    HAS_WEB = False

from utils.session_logger import SessionLogger

# Core imports
# (abbreviated for this commit - full version preserved in previous state)

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

# Logging setup from new config
log_level = getattr(logging, config.LOG_LEVEL.upper(), logging.INFO)
logging.basicConfig(level=log_level, format='[%(asctime)s] %(message)s', datefmt='%H:%M:%S')
logger = logging.getLogger("csi-system")

session_logger = SessionLogger()

def run_pipeline():
    logger.info("="*70)
    logger.info("  WiFi CSI Spatial Intelligence v1.1.0 (Config-driven)")
    logger.info(f"  Using config: ROOM={config.ROOM_NAME}, FRAMES={config.SIMULATION_FRAMES}")
    logger.info("="*70)

    aurora = AuroraAdapter(redis_url=config.REDIS_URL)
    ingest = CSIIngestor(udp_port=config.UDP_PORT)
    calib = CalibrationEngine()
    fusion = FusionEngine()
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

    if HAS_WEB:
        def start_web():
            uvicorn.run(web_app, host="0.0.0.0", port=8000, log_level="warning")
        threading.Thread(target=start_web, daemon=True).start()
        logger.info("Dashboard: http://localhost:8000")

    for frame_idx in range(1, config.SIMULATION_FRAMES + 1):
        raw = generate_test_frame()
        parsed = ingest.parse_csi(raw)
        calibrated = calib.calibrate(parsed)
        voxels = fusion.fuse(calibrated)
        tracks = tracker.update(voxels)
        preds = [predictor.predict(t) for t in tracks]
        behaviors = [behavior.classify(p) for p in preds]
        evs = events.generate(preds)

        memory.update({"tracks": preds, "events": evs})

        if evs:
            decision = agent.decide({"tracks": preds, "events": evs})
            agent.execute(decision)

        if frame_idx % 2 == 0:
            swarm_bridge.send_full_context(preds, evs, behaviors, memory.room_profile)

        state = {
            "frame": frame_idx,
            "tracks": len(preds),
            "events": evs,
            "behaviors": behaviors,
            "voxels": voxels
        }
        dashboard.push(state)

        session_logger.log({"frame": frame_idx, "tracks": len(preds), "events": evs})

        render_voxel_field(voxels, preds)
        time.sleep(config.DEMO_SLEEP)

    logger.info("Demo complete.")

if __name__ == "__main__":
    run_pipeline()