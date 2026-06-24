from prometheus_client import Counter, Histogram, Gauge, start_http_server
import time

# Existing metrics
FRAMES_PROCESSED = Counter('csi_frames_processed_total', 'Total frames processed')
TRACK_COUNT = Gauge('csi_active_tracks', 'Current number of active tracks')
EVENT_COUNT = Counter('csi_events_total', 'Total events generated', ['event_type'])
PROCESSING_TIME = Histogram('csi_frame_processing_seconds', 'Time spent processing each frame')

# New integration health metrics
INTEGRATION_HEALTHY = Gauge('sensing_swarm_integration_healthy', '1 if sensing-swarm integration is healthy, 0 otherwise')
HEARTBEAT_AGE = Gauge('sensing_heartbeat_age_seconds', 'Seconds since last heartbeat from sensing system')
LAST_CONTEXT_AGE = Gauge('sensing_last_context_age_seconds', 'Seconds since last rich context update')

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

    def update_integration_health(self, healthy: bool, heartbeat_age: float = None, context_age: float = None):
        INTEGRATION_HEALTHY.set(1 if healthy else 0)
        if heartbeat_age is not None:
            HEARTBEAT_AGE.set(heartbeat_age)
        if context_age is not None:
            LAST_CONTEXT_AGE.set(context_age)

# Global metrics instance
metrics = None

def get_metrics():
    global metrics
    if metrics is None:
        metrics = Metrics()
    return metrics