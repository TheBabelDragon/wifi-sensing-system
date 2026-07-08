from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple
import time

@dataclass
class NodeState:
    node_id: str
    last_seen: float = field(default_factory=time.time)
    rssi: int = 0
    activity: float = 0.0
    hot_zones: int = 0
    obstruction: bool = False
    position: Optional[Tuple[float, float]] = None   # (x, y) in room coordinates
    packet_count: int = 0


class NodeManager:
    def __init__(self, room_size: Tuple[float, float] = (10.0, 10.0), grid_resolution: int = 20):
        self.nodes: Dict[str, NodeState] = {}
        self.room_size = room_size          # (width, height) in meters
        self.grid_resolution = grid_resolution
        self.probability_field = [[0.0 for _ in range(grid_resolution)] 
                                  for _ in range(grid_resolution)]

    def register_or_update(self, node_id: str, data: dict, position: Optional[Tuple[float, float]] = None):
        """
        Register a new node or update an existing one.
        'data' should contain keys like: rssi, activity, hot_zones, obstruction
        """
        now = time.time()

        if node_id not in self.nodes:
            self.nodes[node_id] = NodeState(node_id=node_id, position=position)

        node = self.nodes[node_id]
        node.last_seen = now
        node.rssi = data.get("rssi", node.rssi)
        node.activity = data.get("activity", node.activity)
        node.hot_zones = data.get("hot_zones", node.hot_zones)
        node.obstruction = data.get("obstruction", node.obstruction)
        node.packet_count += 1

        if position:
            node.position = position

        self._update_probability_field()

    def _update_probability_field(self):
        """
        Very basic spatial fusion.
        Each node contributes activity to nearby grid cells.
        This is a placeholder for proper interpolation later.
        """
        # Reset field
        for y in range(self.grid_resolution):
            for x in range(self.grid_resolution):
                self.probability_field[y][x] = 0.0

        if not self.nodes:
            return

        cell_w = self.room_size[0] / self.grid_resolution
        cell_h = self.room_size[1] / self.grid_resolution

        for node in self.nodes.values():
            if node.position is None:
                continue

            # Simple contribution to nearby cells
            gx = int(node.position[0] / cell_w)
            gy = int(node.position[1] / cell_h)

            # Clamp to grid
            gx = max(0, min(self.grid_resolution - 1, gx))
            gy = max(0, min(self.grid_resolution - 1, gy))

            # Add activity as probability mass
            influence = min(1.0, node.activity * 1.5)
            self.probability_field[gy][gx] = max(
                self.probability_field[gy][gx], influence
            )

            # Light spread to neighbors (very basic)
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    nx, ny = gx + dx, gy + dy
                    if 0 <= nx < self.grid_resolution and 0 <= ny < self.grid_resolution:
                        self.probability_field[ny][nx] = max(
                            self.probability_field[ny][nx], influence * 0.6
                        )

    def get_room_state(self) -> dict:
        total_activity = sum(n.activity for n in self.nodes.values())
        total_hot_zones = sum(n.hot_zones for n in self.nodes.values())
        obstruction_prob = min(1.0, sum(1 for n in self.nodes.values() if n.obstruction) / max(1, len(self.nodes)))

        return {
            "node_count": len(self.nodes),
            "total_activity": round(total_activity, 3),
            "total_hot_zones": total_hot_zones,
            "obstruction_probability": round(obstruction_prob, 3),
            "probability_field": self.probability_field,
            "nodes": {
                nid: {
                    "activity": round(n.activity, 3),
                    "hot_zones": n.hot_zones,
                    "obstruction": n.obstruction,
                    "position": n.position
                }
                for nid, n in self.nodes.items()
            }
        }

    def cleanup_stale_nodes(self, timeout_seconds: float = 30.0):
        now = time.time()
        stale = [nid for nid, n in self.nodes.items() if now - n.last_seen > timeout_seconds]
        for nid in stale:
            del self.nodes[nid]
        if stale:
            self._update_probability_field()
