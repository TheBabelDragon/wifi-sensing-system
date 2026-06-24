class Agent:
    def decide(self, state):
        # Policy-based decision
        if state.get('events'):
            return {'action': 'alert', 'reason': 'occupancy detected'}
        return {'action': 'monitor'}

    def execute(self, action):
        print(f'Executing: {action}')