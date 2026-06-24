import numpy as np

def render_voxel_field(voxel_field, tracks=None, width=24, height=24):
    """Professional ASCII visualization of voxel field with track overlay."""
    field = np.array(voxel_field)
    if field.shape[0] != height or field.shape[1] != width:
        field = np.zeros((height, width))

    grid = []
    for y in range(height):
        row = []
        for x in range(width):
            val = field[y, x]
            if val > 0.75:
                row.append("█")
            elif val > 0.5:
                row.append("▓")
            elif val > 0.25:
                row.append("▒")
            else:
                row.append("░")
        grid.append(row)

    # Overlay tracks with ID
    if tracks:
        for track in tracks:
            cx = int(round(track.get("centroid", [width//2, height//2])[0])) % width
            cy = int(round(track.get("centroid", [width//2, height//2])[1])) % height
            try:
                grid[cy][cx] = "●"
            except IndexError:
                pass

    print("\nVoxel Field (█ high activity, ● = tracked object):")
    print("   " + " ".join(f"{i:2d}" for i in range(width)))
    for i, row in enumerate(grid):
        print(f"{i:2d} " + " ".join(row))
    print()