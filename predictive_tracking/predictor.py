import numpy as np

class Predictor:
    def __init__(self):
        self.states = {}  # track_id -> {'pos': array, 'vel': array, 'cov': matrix}

    def predict(self, track):
        tid = track.get('id')
        if tid not in self.states:
            pos = np.array(track.get('centroid', (0.0, 0.0)))
            self.states[tid] = {
                'pos': pos,
                'vel': np.array([0.0, 0.0]),
                'cov': np.eye(4) * 0.5
            }

        state = self.states[tid]
        # Simple constant velocity prediction
        predicted_pos = state['pos'] + state['vel']

        return {
            'id': tid,
            'predicted_pos': tuple(predicted_pos),
            'centroid': track.get('centroid'),
            'speed': float(np.linalg.norm(state['vel'])),
            'velocity': tuple(state['vel'])
        }

    def update(self, observation):
        tid = observation.get('id')
        if tid in self.states:
            pos = np.array(observation.get('centroid', (0.0, 0.0)))
            state = self.states[tid]
            # Simple correction (like basic Kalman update)
            innovation = pos - state['pos']
            state['pos'] = state['pos'] + 0.7 * innovation
            state['vel'] = 0.3 * state['vel'] + 0.7 * innovation
        return observation