import numpy as np

class CalibrationEngine:
    def __init__(self):
        self.anchors = {}
        self.baseline = None
        self.scale_factor = 1.0

    def register_anchors(self, nodes):
        self.anchors = nodes

    def build_baseline(self, csi_stream):
        self.baseline = np.mean([f['csi'] for f in csi_stream], axis=0)

    def calibrate(self, frame):
        return frame  # placeholder

    def to_metric(self, voxel_coords):
        return [c * self.scale_factor for c in voxel_coords]