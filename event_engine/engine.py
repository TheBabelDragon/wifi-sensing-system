class EventEngine:
    def __init__(self):
        self.last_events = []

    def generate(self, tracks):
        events = []
        num_tracks = len(tracks)

        if num_tracks > 0:
            events.append("ROOM_OCCUPIED")

        rapid_count = sum(1 for t in tracks if t.get("speed", 0) > 1.8)
        if rapid_count > 0:
            events.append("RAPID_MOVEMENT")

        if num_tracks >= 3:
            events.append("CROWD_DETECTED")

        # Simple anomaly: sudden appearance of many tracks
        if num_tracks > 4 and len(self.last_events) > 0 and "ROOM_OCCUPIED" not in self.last_events:
            events.append("ANOMALY_SUDDEN_CROWD")

        self.last_events = events
        return events