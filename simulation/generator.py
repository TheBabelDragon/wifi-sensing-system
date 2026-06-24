import random

def generate_test_frame(node_id="esp32_test"):
    """Generate realistic simulated CSI frame for testing."""
    csi = [random.uniform(0.2, 0.9) for _ in range(32)]
    rssi = random.randint(-75, -45)
    
    # Occasionally simulate movement
    if random.random() > 0.7:
        links = [{"node_a": node_id, "node_b": "anchor_1", "weight": random.uniform(0.6, 1.0)}]
    else:
        links = []

    return {
        "node": node_id,
        "csi": csi,
        "rssi": rssi,
        "links": links,
        "timestamp": "simulated"
    }