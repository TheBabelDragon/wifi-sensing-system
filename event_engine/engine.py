class EventEngine:
    def generate(self, tracks):
        events = []
        if len(tracks) > 0:
            events.append('ROOM_OCCUPIED')
        if any(t.get('speed', 0) > 2.0 for t in tracks):
            events.append('RAPID_MOVEMENT')
        return events