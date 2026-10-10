"""Compares a raytracer render with a reference render of the same scene.

    python scripts/compare_with_reference.py OURS REFERENCE SHEET [LABEL]

Prints the mean colour of each image and their mean absolute difference, in
8-bit units, and writes SHEET: the two images side by side with their
absolute difference, scaled 4x so small errors show.

The mean colour is the number to judge by. A path tracer's per-pixel noise
differs between any two renderers, and between two seeds of one, so the
pixel-wise difference never reaches zero; a texel landing in the wrong place
moves whole regions of it instead, and shows in the sheet as shapes.
"""

import sys

import numpy as np
from PIL import Image, ImageDraw

ours_path, ref_path, sheet_path = sys.argv[1:4]
label = sys.argv[4] if len(sys.argv) > 4 else "reference"

ours = Image.open(ours_path).convert("RGB")
ref = Image.open(ref_path).convert("RGB")
if ours.size != ref.size:
    raise SystemExit(f"sizes differ: {ours.size} against {ref.size}")

a = np.asarray(ours, dtype=np.float64)
b = np.asarray(ref, dtype=np.float64)
diff = np.abs(a - b)
print(f"mean RGB, raytracer  {a.reshape(-1, 3).mean(0).round(2)}")
print(f"mean RGB, {label:<10s} {b.reshape(-1, 3).mean(0).round(2)}")
print(f"mean |difference|    {diff.mean():.2f} / 255")

width, height = ours.size
sheet = Image.new("RGB", (3 * width, height + 24), "white")
panels = [(ours, "raytracer"), (ref, label),
          (Image.fromarray(np.clip(diff * 4, 0, 255).astype(np.uint8)),
           "|difference| x4")]
for i, (image, title) in enumerate(panels):
    sheet.paste(image, (i * width, 24))
    ImageDraw.Draw(sheet).text((i * width + 8, 6), title, fill="black")
sheet.save(sheet_path)
print(f"wrote {sheet_path}")
