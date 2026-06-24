class DashboardServer:
    def __init__(self):
        self.state = {}
        self.history = []

    def push(self, state):
        self.state = state
        self.history.append(state)
        if len(self.history) > 20:
            self.history.pop(0)
        # In real implementation this would push via WebSocket
        print(f"[Dashboard] Updated: Frame {state.get('frame')}, Tracks: {state.get('tracks')}, Events: {state.get('events')}")

    def render(self):
        return {
            "current": self.state,
            "recent_history": self.history[-5:]
        }