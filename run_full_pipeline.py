import sys
sys.path.append('.')
import os
import time
import logging
import threading
import traceback

from config import config

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

try:
    from dashboard_web.app import app as web_app
    import uvicorn
    HAS_WEB = True
except ImportError:
    HAS_WEB = False

try:
    from observability.metrics import get_metrics
    HAS_METRICS = True
except ImportError:
    HAS_METRICS = False

try:
    from sensing.command_listener import SensingCommandListener
    HAS_COMMAND_LISTENER = True
except ImportError:
    HAS_COMMAND_LISTENER = False

from utils.session_logger import SessionLogger

# Core imports
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
from bridges.swarm_bridge import SwarmBridge
from visualization.text_viz import render_voxel_field

logging.basicConfig(level=logging.INFO, format='[%(asctime)s] %(message)s')
logger = logging.getLogger("csi-system")

session_logger = SessionLogger()

if HAS_METRICS:
    metrics = get_metrics()

def handle_gateway_data(data):
    msg_type = data.get("type", "unknown")
    logger.info(f"[Hybrid] Gateway message received: {msg_type}")


def run_pipeline():
    logger.info("="*70)
    logger.info("  WiFi CSI Spatial Intelligence v2.0 - Real + Simulation Mode")
    logger.info("="*70)

    aurora = AuroraAdapter(redis_url=config.REDIS_URL)
    ingest = CSIIngestor(udp_port=getattr(config, 'UDP_PORT', 4210))
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

    if HAS_COMMAND_LISTENER:
        def start_listener():
            listener = SensingCommandListener(redis_url=config.REDIS_URL)
            listener.listen()
        threading.Thread(target=start_listener, daemon=True).start()
        logger.info("Bidirectional command listener active")

    last_heartbeat = time.time()
    last_status_log = time.time()

    logger.info("Starting main ingestion loop (real UDP + fallback simulation)...")

    # Main loop using proper ingestor
    for frame_idx in range(1, config.SIMULATION_FRAMES + 1):
        start_time = time.time()

        try:
            # === CORRECT DATA FLOW ===
            raw = ingest.read_packet()          # Read from UDP (real ESP32 nodes)

            if raw is None:
                # Fallback to simulation only if no real packet arrived
                from simulation.generator import generate_test_frame
                raw = generate_test_frame(node_id=f"sim_{config.ROOM_NAME}")

            # Determine operating mode every single frame
            mode = "REAL" if raw and raw.get("timestamp") != "simulated" else "SIM"

            parsed = ingest.parse_packet(raw)

            if parsed is None:
                continue

            if parsed.get("source") == "meshtastic_gateway":
                handle_gateway_data(parsed["data"])
                continue

            # === Normal WiFi CSI Processing Pipeline ===
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

            if frame_idx % 2 == 0 or len(preds) >= 2:
                swarm_bridge.send_full_context(preds, evs, behaviors, memory.room_profile)

            if time.time() - last_heartbeat > 8:
                swarm_bridge.send_heartbeat()
                last_heartbeat = time.time()

            if time.time() - last_status_log > 15:
                logger.info(f"[Pipeline] {mode} | Frame {frame_idx} | Tracks: {len(preds)} | Events: {len(evs)}")
                last_status_log = time.time()

            # Debug logging for per-frame details
            processing_time = time.time() - start_time
            logger.debug(
                f"Frame {frame_idx:04d} | Mode: {mode} | "
                f"Tracks: {len(preds):2d} | Events: {len(evs):2d} | "
                f"Processing: {processing_time*1000:.1f} ms"
            )

            state = {
                "frame": frame_idx,
                "tracks": len(preds),
                "events": evs,
                "behaviors": behaviors,
                "voxels": voxels
            }
            dashboard.push(state)

            if HAS_METRICS:
                metrics.record_frame(processing_time, len(preds), evs)

            session_logger.log({
                "frame": frame_idx,
                "tracks": len(preds),
                "events": evs,
                "mode": mode
            })

            render_voxel_field(voxels, preds)

        except Exception as e:
            logger.error(f"Error processing frame {frame_idx}: {e}")
            logger.debug(traceback.format_exc())
            # Continue to next frame instead of crashing the whole pipeline
            continue

        time.sleep(config.DEMO_SLEEP)

    logger.info("Pipeline finished.")


if __name__ == "__main__":
    run_pipeline()
