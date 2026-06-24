from prometheus_client import Counter, Histogram, Gauge, start_http_server
import time

# Metrics
FRAMES_PROCESSED = Counter('csi_frames_processed_total', 'Total frames processed')
TRACK_COUNT = Gauge('csi_active_tracks', 'Current number of active tracks')
EVENT_COUNT = Counter('csi_events_total', 'Total events generated', ['event_type'])
PROCESSING_TIME = Histogram('csi_frame_processing_seconds', 'Time spent processing each frame')

class Metrics:
    def __init__(self, port: int = 8001):
        self.port = port
        start_http_server(port)
        print(f"[Metrics] Prometheus metrics available at http://localhost:{port}/metrics")

    def record_frame(self, processing_time: float, num_tracks: int, events: list):
        FRAMES_PROCESSED.inc()
        TRACK_COUNT.set(num_tracks)
        PROCESSING_TIME.observe(processing_time)

        for event in events:
            EVENT_COUNT.labels(event_type=event).inc()

# Global metrics instance (lazy init)
metrics = None

def get_metrics():
    global metrics
    if metrics is None:
        metrics = Metrics()
    return metrics