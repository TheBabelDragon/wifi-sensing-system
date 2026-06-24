import numpy as np

def render_voxel_field(voxel_field, tracks=None, width=24, height=24):
    """Simple ASCII visualization of the voxel probability field + tracks."""
    field = np.array(voxel_field)
    if field.shape != (height, width):
        field = np.zeros((height, width))

    # Create grid
    grid = []
    for y in range(height):
        row = []
        for x in range(width):
            val = field[y, x]
            if val > 0.7:
                row.append("█")
            elif val > 0.4:
                row.append("▓")
            elif val > 0.2:
                row.append("▒")
            else:
                row.append("░")
        grid.append(row)

    # Overlay tracks
    if tracks:
        for track in tracks:
            cx, cy = track.get("centroid", (width//2, height//2))
            cx = int(round(cx)) % width
            cy = int(round(cy)) % height
            try:
                grid[cy][cx] = "●"
            except:
                pass

    # Print
    print("\nVoxel Field + Tracks (● = tracked object):")
    print("  " + " ".join([str(i%10) for i in range(width)]))
    for i, row in enumerate(grid):
        print(f"{i:2d} " + " ".join(row))
    print()