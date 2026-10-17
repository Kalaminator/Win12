# usage: w17edge.py <png> <x> [bottom_px=120]
# Scans one screenshot column in the bottom band and finds the biggest luminance step between neighbouring rows
# (the island's top edge over a plain background). Prints edge_y=<row> step=<|dL|> mean_inside=<mean L of the 10
# rows under edge_y>. L = Rec.709 luminance 0..1. Readable glass passes at step >= .06 (spec 12.4).
import sys
from PIL import Image

img = Image.open(sys.argv[1]).convert("RGB")
x, band = int(sys.argv[2]), int(sys.argv[3]) if len(sys.argv) > 3 else 120
w, h = img.size
px = img.load()
L = lambda y: (lambda r, g, b: (.2126 * r + .7152 * g + .0722 * b) / 255)(*px[x, y])
rows = range(max(1, h - band), h)
# the step from row y-1 (outside) to row y (the edge line itself)
edge = max(rows, key=lambda y: abs(L(y) - L(y - 1)))
step = abs(L(edge) - L(edge - 1))
inside = [L(y) for y in range(edge + 1, min(h, edge + 11))]
mean = sum(inside) / len(inside) if inside else L(edge)
print(f"edge_y={edge} step={step:.3f} mean_inside={mean:.3f}")
