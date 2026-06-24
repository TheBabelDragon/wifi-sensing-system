import numpy as np

class TrackerEngine:
    def __init__(self):
        self.tracks = []
        self.next_id = 1

    def _detect_blobs(self, voxel_field):
        # Simple threshold-based blob detection
        field = np.array(voxel_field)
        threshold = 0.3
        blobs = []
        for x in range(field.shape[0]):
            for y in range(field.shape[1]):
                if field[x, y] > threshold:
                    blobs.append((x, y))
        return blobs

    def _compute_centroid(self, blob_points):
        if not blob_points:
            return (0, 0)
        xs = [p[0] for p in blob_points]
        ys = [p[1] for p in blob_points]
        return (np.mean(xs), np.mean(ys))

    def update(self, voxel_field):
        blobs = self._detect_blobs(voxel_field)
        new_tracks = []
        for blob in blobs:
            centroid = self._compute_centroid([blob])
            # Simple assignment (in real system use Hungarian or distance matching)
            track = {
                "id": self.next_id,
                "centroid": centroid,
                "speed": 0.5,  # placeholder velocity
                "last_seen": "now"
            }
            new_tracks.append(track)
            self.next_id += 1
        self.tracks = new_tracks
        return self.tracks