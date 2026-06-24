import numpy as np

class Predictor:
    def __init__(self):
        self.track_states = {}  # id -> {'pos': (x,y), 'vel': (vx,vy)}

    def predict(self, track):
        track_id = track.get('id', 0)
        if track_id not in self.track_states:
            self.track_states[track_id] = {
                'pos': track.get('centroid', (0,0)),
                'vel': (0.1, 0.1)
            }
        state = self.track_states[track_id]
        # Simple constant velocity prediction
        new_x = state['pos'][0] + state['vel'][0]
        new_y = state['pos'][1] + state['vel'][1]
        predicted = {
            'id': track_id,
            'predicted_pos': (new_x, new_y),
            'centroid': track.get('centroid'),
            'speed': np.sqrt(state['vel'][0]**2 + state['vel'][1]**2)
        }
        return predicted

    def update(self, observation):
        # In real Kalman this would correct velocity
        track_id = observation.get('id')
        if track_id in self.track_states:
            self.track_states[track_id]['pos'] = observation.get('centroid', (0,0))
        return observation