from typing import Dict, Any, Optional
import numpy as np

from .feature_extractor import FeatureExtractor
from .inference_engine import InferenceEngine

class MLPipeline:
    """
    High-level ML pipeline that combines feature extraction and inference.
    Can be used locally near sensing or called by aurora-swarm-btc workers.
    """

    def __init__(self, model_path: Optional[str] = None):
        self.feature_extractor = FeatureExtractor()
        self.inference_engine = InferenceEngine(model_path=model_path)

    def process_room_state(self, room_state: Dict[str, Any]) -> Dict[str, Any]:
        """
        Full pipeline: extract features from RoomState -> run inference.
        """
        features = self.feature_extractor.extract_from_room_state(room_state)

        if len(features) == 0:
            return {
                "prediction": None,
                "confidence": 0.0,
                "features": []
            }

        result = self.inference_engine.predict(features)
        result["features"] = features.tolist()
        return result

    def process_node(self, node_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Per-node inference (lighter weight).
        """
        features = self.feature_extractor.extract_per_node(node_data)
        result = self.inference_engine.predict(features)
        result["features"] = features.tolist()
        return result
