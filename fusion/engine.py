import numpy as np

class FusionEngine:
    def __init__(self, grid_size=20):
        self.grid_size = grid_size
        self.voxel_field = np.zeros((grid_size, grid_size))

    def _distance_decay(self, dist, sigma=3.0):
        return np.exp(-dist**2 / (2 * sigma**2))

    def _apply_influence(self, link, grid_size):
        influence = np.zeros((grid_size, grid_size))
        # Simplified: assume link has 'weight' and approximate position
        weight = link.get('weight', 1.0)
        cx, cy = grid_size // 2, grid_size // 2  # center influence
        for x in range(grid_size):
            for y in range(grid_size):
                dist = np.sqrt((x - cx)**2 + (y - cy)**2)
                influence[x, y] = weight * self._distance_decay(dist)
        return influence

    def fuse(self, frame):
        self.voxel_field = np.zeros((self.grid_size, self.grid_size))
        links = frame.get('links', []) if isinstance(frame, dict) else []
        if not links:
            # Create synthetic link from CSI strength
            strength = np.mean(frame.get('csi', [0.5]*32)) if isinstance(frame, dict) else 0.5
            links = [{'weight': strength}]

        for link in links:
            self.voxel_field += self._apply_influence(link, self.grid_size)

        # Normalize
        max_val = np.max(self.voxel_field)
        if max_val > 0:
            self.voxel_field /= max_val
        return self.voxel_field.tolist()