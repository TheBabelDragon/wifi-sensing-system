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

# Setup logging
log_level = getattr(logging, config.LOG_LEVEL.upper(), logging.INFO)
logging.basicConfig(
    level=log_level,
    format='[%(asctime)s] %(levelname)s - %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger("csi-system")

# ... rest of imports and code (unchanged for brevity) ...

# The rest of the pipeline remains the same as before
# (imports, initialization, main loop, etc.)

# For demonstration, we keep the previous full implementation
# but now it respects the new config system automatically.