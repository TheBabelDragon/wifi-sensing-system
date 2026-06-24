import numpy as np

class InteractionGraph:
    def build(self, tracks):
        graph = {}
        n = len(tracks)
        for i in range(n):
            for j in range(i + 1, n):
                t1 = tracks[i]
                t2 = tracks[j]
                c1 = np.array(t1.get("centroid", (0, 0)))
                c2 = np.array(t2.get("centroid", (0, 0)))
                dist = float(np.linalg.norm(c1 - c2))
                relation = "close" if dist < 2.0 else ("medium" if dist < 4.0 else "distant")
                graph[(i, j)] = {
                    "distance": round(dist, 2),
                    "relation": relation,
                    "interaction_score": max(0, 1.0 - (dist / 5.0))
                }
        return graph