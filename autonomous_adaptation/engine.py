class AdaptationEngine:
    def __init__(self):
        self.parameters = {
            "smoothness": 0.5,
            "sensitivity": 0.8,
            "prediction_horizon": 3
        }
        self.history = []

    def adjust(self, metrics):
        error = metrics.get("error", 0.0)
        if error > 0.4:
            self.parameters["smoothness"] = max(0.1, self.parameters["smoothness"] - 0.05)
            self.parameters["sensitivity"] = min(1.0, self.parameters["sensitivity"] + 0.05)
        elif error < 0.15:
            self.parameters["smoothness"] = min(0.9, self.parameters["smoothness"] + 0.03)
        self.history.append({"metrics": metrics, "params": self.parameters.copy()})
        return self.parameters.copy()