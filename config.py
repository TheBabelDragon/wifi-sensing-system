"""Central configuration for WiFi CSI Spatial Intelligence System."""

import os

class Config:
    # === General ===
    NODE_ID = os.getenv("NODE_ID", "esp32_mining_hall_01")
    ROOM_NAME = os.getenv("SENSING_ROOM_NAME", "mining_hall")

    # === UDP / Ingestion ===
    UDP_PORT = int(os.getenv("UDP_PORT", 4210))

    # === Pipeline ===
    SIMULATION_FRAMES = int(os.getenv("SIMULATION_FRAMES", 8))
    DEMO_SLEEP = float(os.getenv("DEMO_SLEEP", 0.35))

    # === Redis / Swarm ===
    REDIS_URL = os.getenv("REDIS_URL", None)

    # === Tracking ===
    MAX_TRACK_DISTANCE = float(os.getenv("MAX_TRACK_DISTANCE", 3.0))

    # === Agent ===
    AGENT_COOLDOWN = int(os.getenv("AGENT_COOLDOWN", 25))

    # === Logging ===
    VERBOSE = os.getenv("VERBOSE", "true").lower() == "true"

config = Config()