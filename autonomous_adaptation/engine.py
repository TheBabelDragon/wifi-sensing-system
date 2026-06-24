class AdaptationEngine:
    def __init__(self):
        self.parameters = {'smoothness': 0.5}

    def adjust(self, metrics):
        if metrics.get('error', 0) > 0.3:
            self.parameters['smoothness'] = max(0.1, self.parameters['smoothness'] - 0.1)
        return self.parameters