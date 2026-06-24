from simulation.generator import generate_test_frame
from ingestion.ingestor import CSIIngestor
from fusion.engine import FusionEngine
from tracking.tracker import TrackerEngine
from visualization.text_viz import render_voxel_field

print("=== Multi-Node CSI Demo ===\n")

ingest = CSIIngestor()
fusion = FusionEngine(grid_size=20)
tracker = TrackerEngine()

for i in range(6):
    print(f"\n--- Combined Frame {i+1} ---")
    
    # Simulate two nodes
    frame1 = generate_test_frame("esp32_hall_left")
    frame2 = generate_test_frame("esp32_hall_right")
    
    parsed1 = ingest.parse_csi(frame1)
    parsed2 = ingest.parse_csi(frame2)
    
    # Simple fusion of two sources
    voxel1 = fusion.fuse(parsed1)
    voxel2 = fusion.fuse(parsed2)
    
    # Combine (very basic)
    combined = [[max(voxel1[y][x], voxel2[y][x]) for x in range(20)] for y in range(20)]
    
    tracks = tracker.update(combined)
    render_voxel_field(combined, tracks, width=20, height=20)
    
    print(f"Active tracks: {len(tracks)}")