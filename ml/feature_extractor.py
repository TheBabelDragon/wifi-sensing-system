from typing import Dict, Any, List
import numpy as np

class FeatureExtractor:
    """
    Extracts ML-ready features from NodeManager RoomState or raw per-node data.
    Designed to run close to sensing while remaining usable by aurora-swarm-btc workers.
    """

    def __init__(self):
        pass

    def extract_from_room_state(self, room_state: Dict[str, Any]) -> np.ndarray:
        """
        Convert a full RoomState dict into a feature vector.
        """
        nodes = room_state.get("nodes", {})
        if not nodes:
            return np.array([])

        features = []
        for node_id, node_data in nodes.items():
            feat = [
                node_data.get("activity", 0.0),
                node_data.get("hot_zones", 0),
                node_data.get("movement_intensity", 0.0),
                node_data.get("confidence", 0.5),
                1.0 if node_data.get("obstruction", False) else 0.0,
            ]
            features.append(feat)

        # Simple aggregation for now (can be improved later)
        features = np.array(features)
        return np.concatenate([
            features.mean(axis=0),
            features.std(axis=0),
            [room_state.get("total_activity", 0.0)],
            [room_state.get("obstruction_probability", 0.0)],
        ])

    def extract_per_node(self, node_data: Dict[str, Any]) -> np.ndarray:
        """
        Extract features for a single node (useful for per-node models).
        """
        return np.array([
            node_data.get("activity", 0.0),
            node_data.get("hot_zones", 0),
            node_data.get("movement_intensity", 0.0),
            node_data.get("confidence", 0.5),
            1.0 if node_data.get("obstruction", False) else 0.0,
        ])
