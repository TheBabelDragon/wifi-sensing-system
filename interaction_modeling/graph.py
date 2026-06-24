class InteractionGraph:
    def build(self, tracks):
        graph = {}
        for i, t1 in enumerate(tracks):
            for j, t2 in enumerate(tracks):
                if i != j:
                    dist = ((t1.get('centroid', [0])[0] - t2.get('centroid', [0])[0])**2 + (t1.get('centroid', [0])[1] - t2.get('centroid', [0])[1])**2)**0.5
                    graph[(i, j)] = {'distance': dist, 'relation': 'proximity' if dist < 2 else 'distant'}
        return graph