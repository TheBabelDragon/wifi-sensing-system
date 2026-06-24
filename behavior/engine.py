class BehaviorEngine:
    def classify(self, track):
        speed = track.get('speed', 0.0)
        if speed < 0.2:
            return 'idle'
        elif speed < 1.0:
            return 'walking'
        return 'rapid_motion'