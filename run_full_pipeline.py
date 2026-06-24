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

# ... (imports remain the same)

from aurora.adapter import AuroraAdapter
# ... other imports ...

logging.basicConfig(level=logging.INFO, format='[%(asctime)s] %(message)s')
logger = logging.getLogger("csi-system")

session_logger = SessionLogger()

def run_pipeline():
    logger.info("="*70)
    logger.info("  WiFi CSI Spatial Intelligence System v1.1.0")
    logger.info("  Session logging enabled → " + session_logger.get_log_path())
    logger.info("="*70)

    # ... initialization code stays similar ...

    aurora = AuroraAdapter(redis_url=config.REDIS_URL)
    # ... rest of initialization ...

    if HAS_WEB:
        def start_web():
            uvicorn.run(web_app, host="0.0.0.0", port=8000, log_level="warning")
        threading.Thread(target=start_web, daemon=True).start()
        logger.info("Central dashboard: http://localhost:8000")

    for frame_idx in range(1, config.SIMULATION_FRAMES + 1):
        # ... existing frame processing ...

        state = {
            "frame": frame_idx,
            "room": config.ROOM_NAME,
            "tracks": len(preds),
            "events": evs,
            "behaviors": behaviors
        }
        dashboard.push(state)

        # Log every frame
        session_logger.log({
            "timestamp": time.time(),
            "frame": frame_idx,
            "tracks": len(preds),
            "events": evs,
            "behaviors": behaviors
        })

        render_voxel_field(voxels, preds)
        time.sleep(config.DEMO_SLEEP)

    logger.info("\nSession saved to: " + session_logger.get_log_path())
    logger.info("Demo complete.")

if __name__ == "__main__":
    run_pipeline()