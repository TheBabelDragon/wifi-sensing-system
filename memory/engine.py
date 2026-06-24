class MemoryEngine:
    def __init__(self):
        self.room_profile = {}
        self.history = []

    def update(self, snapshot):
        self.history.append(snapshot)
        self.room_profile['occupancy'] = len(snapshot.get('tracks', []))

    def query(self, pattern):
        return [h for h in self.history if pattern in str(h)]