import numpy as np

class CalibrationEngine:
    def __init__(self):
        self.anchors = {}
        self.baseline = None
        self.scale_factor = 1.0  # meters per voxel unit
        self.room_bounds = (0, 10, 0, 10)  # x_min, x_max, y_min, y_max

    def register_anchors(self, nodes):
        self.anchors = nodes
        print(f"[Calibration] Registered {len(nodes)} anchors")

    def build_baseline(self, csi_stream):
        if not csi_stream:
            return
        csi_arrays = [np.array(f.get('csi', [0]*32)) for f in csi_stream if f]
        if csi_arrays:
            self.baseline = np.mean(csi_arrays, axis=0)
            print("[Calibration] Baseline built from stream")

    def calibrate(self, frame):
        if self.baseline is None or not frame:
            return frame
        csi = np.array(frame.get('csi', [0]*32))
        calibrated_csi = csi - self.baseline
        frame['csi'] = calibrated_csi.tolist()
        return frame

    def to_metric(self, voxel_coords):
        # Simple linear mapping
        x = voxel_coords[0] * self.scale_factor
        y = voxel_coords[1] * self.scale_factor
        return (x, y)