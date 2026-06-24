import numpy as np

class CalibrationEngine:
    def __init__(self):
        self.anchors = {}
        self.baseline = None
        self.scale_factor = 0.5  # voxel units to meters
        self.drift_threshold = 0.15
        self.last_drift_check = None

    def register_anchors(self, nodes):
        self.anchors = nodes

    def build_baseline(self, csi_stream):
        if not csi_stream:
            return
        arrays = []
        for f in csi_stream:
            if f and 'csi' in f:
                arrays.append(np.array(f['csi']))
        if arrays:
            self.baseline = np.mean(arrays, axis=0)

    def calibrate(self, frame):
        if self.baseline is None or frame is None:
            return frame
        csi = np.array(frame.get('csi', []))
        if len(csi) == len(self.baseline):
            frame['csi'] = (csi - self.baseline).tolist()
        # Simple drift detection
        if self.last_drift_check is not None:
            drift = np.mean(np.abs(csi - self.last_drift_check))
            if drift > self.drift_threshold:
                frame['drift_detected'] = True
        self.last_drift_check = csi
        return frame

    def to_metric(self, voxel_coords):
        return (voxel_coords[0] * self.scale_factor, voxel_coords[1] * self.scale_factor)