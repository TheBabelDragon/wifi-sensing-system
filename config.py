"""Central configuration system supporting YAML + environment variables."""

import os
import yaml
from dataclasses import dataclass, field
from typing import Optional

@dataclass
class Config:
    # === General ===
    NODE_ID: str = "esp32_mining_hall_01"
    ROOM_NAME: str = "mining_hall"

    # === UDP / Ingestion ===
    UDP_PORT: int = 4210

    # === Pipeline ===
    SIMULATION_FRAMES: int = 8
    DEMO_SLEEP: float = 0.35

    # === Redis / Swarm ===
    REDIS_URL: Optional[str] = None

    # === Tracking ===
    MAX_TRACK_DISTANCE: float = 3.0

    # === Agent ===
    AGENT_COOLDOWN: int = 25

    # === Logging ===
    LOG_LEVEL: str = "INFO"
    STRUCTURED_LOGGING: bool = False

    @classmethod
    def from_yaml(cls, path: str = "config.yaml"):
        if os.path.exists(path):
            with open(path, "r") as f:
                data = yaml.safe_load(f) or {}
            return cls(**{k: v for k, v in data.items() if hasattr(cls, k)})
        return cls()

    def apply_env_overrides(self):
        for key in self.__dataclass_fields__:
            env_val = os.getenv(key)
            if env_val is not None:
                field_type = self.__dataclass_fields__[key].type
                if field_type == bool:
                    setattr(self, key, env_val.lower() in ("true", "1", "yes"))
                elif field_type == int:
                    setattr(self, key, int(env_val))
                elif field_type == float:
                    setattr(self, key, float(env_val))
                else:
                    setattr(self, key, env_val)

# Global config instance
config = Config.from_yaml()
config.apply_env_overrides()