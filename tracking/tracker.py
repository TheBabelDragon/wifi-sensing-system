import numpy as np

class TrackerEngine:
    def __init__(self, max_distance=3.0):
        self.tracks = []
        self.next_id = 1
        self.max_distance = max_distance  # for association

    def _euclidean_distance(self, p1, p2):
        return np.sqrt((p1[0] - p2[0])**2 + (p1[1] - p2[1])**2)

    def _detect_blobs(self, voxel_field, threshold=0.25):
        field = np.array(voxel_field)
        blobs = []
        for x in range(field.shape[0]):
            for y in range(field.shape[1]):
                if field[x, y] > threshold:
                    blobs.append((x, y, field[x, y]))
        return blobs

    def _compute_centroid(self, points):
        if not points:
            return (0.0, 0.0)
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        return (float(np.mean(xs)), float(np.mean(ys)))

    def update(self, voxel_field):
        blobs = self._detect_blobs(voxel_field)
        current_centroids = [self._compute_centroid([b[:2]]) for b in blobs]

        new_tracks = []
        used = set()

        for centroid in current_centroids:
            best_match = None
            best_dist = self.max_distance
            for i, track in enumerate(self.tracks):
                if i in used:
                    continue
                dist = self._euclidean_distance(centroid, track['centroid'])
                if dist < best_dist:
                    best_dist = dist
                    best_match = i

            if best_match is not None:
                # Update existing track
                track = self.tracks[best_match]
                track['centroid'] = centroid
                track['last_seen'] = 'now'
                # Simple velocity estimate
                if 'prev_centroid' in track:
                    vx = centroid[0] - track['prev_centroid'][0]
                    vy = centroid[1] - track['prev_centroid'][1]
                    track['speed'] = float(np.sqrt(vx**2 + vy**2))
                track['prev_centroid'] = centroid
                new_tracks.append(track)
                used.add(best_match)
            else:
                # New track
                new_track = {
                    "id": self.next_id,
                    "centroid": centroid,
                    "speed": 0.0,
                    "last_seen": "now",
                    "prev_centroid": centroid
                }
                new_tracks.append(new_track)
                self.next_id += 1

        self.tracks = new_tracks
        return self.tracks