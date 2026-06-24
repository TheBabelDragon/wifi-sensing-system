import json
import os
from datetime import datetime

class SessionLogger:
    def __init__(self, log_dir="logs"):
        os.makedirs(log_dir, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.log_file = os.path.join(log_dir, f"session_{timestamp}.jsonl")

    def log(self, data: dict):
        with open(self.log_file, "a") as f:
            f.write(json.dumps(data) + "\n")

    def get_log_path(self):
        return self.log_file