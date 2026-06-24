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
# (keeping imports clean)

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

logging.basicConfig(level=logging.INFO, format='[%(asctime)s] %(message)s')
logger = logging.getLogger("csi-system")

session_logger = SessionLogger()

def run_pipeline():
    logger.info("="*70)
    logger.info("  WiFi CSI Spatial Intelligence System v1.1.0")
    logger.info("  Session logs → " + session_logger.get_log_path())
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
        logger.info("Central dashboard running at http://localhost:8000")

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
        params = adaptation.adjust({"error": round(0.1 + frame_idx * 0.03, 2)})

        if evs:
            decision = agent.decide({"tracks": preds, "events": evs})
            agent.execute(decision)

        state = {
            "frame": frame_idx,
            "tracks": len(preds),
            "events": evs,
            "behaviors": behaviors,
            "voxels": voxels
        }
        dashboard.push(state)

        # Send periodic health to swarm
        if frame_idx % 3 == 0:
            swarm_bridge.send_thermal_context(round(len(preds) * 0.4, 2))

        session_logger.log({"frame": frame_idx, "tracks": len(preds), "events": evs})

        render_voxel_field(voxels, preds)
        time.sleep(config.DEMO_SLEEP)

    logger.info("\nSession complete. Logs saved.")

if __name__ == "__main__":
    run_pipeline()