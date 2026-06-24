import numpy as np

class FusionEngine:
    def __init__(self, grid_size=24):
        self.grid_size = grid_size

    def _gaussian_influence(self, cx, cy, sigma=4.0):
        x = np.arange(self.grid_size)
        y = np.arange(self.grid_size)
        xx, yy = np.meshgrid(x, y)
        return np.exp(-((xx - cx)**2 + (yy - cy)**2) / (2 * sigma**2))

    def fuse(self, frame):
        field = np.zeros((self.grid_size, self.grid_size))
        links = frame.get('links', []) if isinstance(frame, dict) else []

        if not links:
            # Fallback: use average CSI strength as center influence
            strength = np.mean(frame.get('csi', [0.5]*32)) if isinstance(frame, dict) else 0.5
            cx = self.grid_size // 2 + np.random.uniform(-2, 2)
            cy = self.grid_size // 2 + np.random.uniform(-2, 2)
            field += strength * self._gaussian_influence(cx, cy)
        else:
            for link in links:
                weight = link.get('weight', 0.8)
                cx = self.grid_size // 2 + np.random.uniform(-3, 3)
                cy = self.grid_size // 2 + np.random.uniform(-3, 3)
                field += weight * self._gaussian_influence(cx, cy)

        # Normalize to [0, 1]
        maxv = np.max(field)
        if maxv > 0:
            field = field / maxv
        return field.tolist()