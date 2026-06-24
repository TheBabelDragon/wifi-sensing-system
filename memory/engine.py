class MemoryEngine:
    def __init__(self):
        self.room_profile = {
            "total_occupancy_events": 0,
            "avg_occupancy": 0,
            "common_behaviors": {}
        }
        self.history = []

    def update(self, snapshot):
        self.history.append(snapshot)
        tracks = snapshot.get("tracks", [])
        self.room_profile["total_occupancy_events"] += len(tracks)
        if tracks:
            current_avg = len(tracks)
            prev_avg = self.room_profile.get("avg_occupancy", 0)
            self.room_profile["avg_occupancy"] = (prev_avg * 0.9 + current_avg * 0.1)

    def query(self, pattern):
        results = []
        for h in self.history[-50:]:  # last 50 snapshots
            if pattern.lower() in str(h).lower():
                results.append(h)
        return results