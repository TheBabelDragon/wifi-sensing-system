class DashboardServer:
    def __init__(self):
        self.state = {}

    def push(self, state):
        self.state = state
        print('Dashboard update:', state)

    def render(self):
        return self.state