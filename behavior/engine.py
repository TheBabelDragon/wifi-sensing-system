class BehaviorEngine:
    def classify(self, track):
        speed = track.get('speed', 0.0)
        if isinstance(speed, (list, tuple)):
            speed = np.mean(speed) if 'np' in globals() else sum(speed)/len(speed)
        if speed < 0.2:
            return "idle"
        elif speed < 1.0:
            return "walking"
        elif speed < 2.5:
            return "fast_walking"
        return "rapid_motion"