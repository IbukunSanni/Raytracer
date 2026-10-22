"""Draws docs/images/furnace-diagram.{png,svg}: why the furnace sphere vanishes.

    python scripts/furnace_diagram.py

A cross-section of tests/scenes/furnace.lua: a camera, an albedo-1 sphere,
and an environment of radiance L in every direction. One camera ray misses
and reads L straight off the environment. One hits, bounces once -- a convex
sphere cannot see itself, so the bounce always escapes -- and reads
1 x L = L. Both pixels agree, so the sphere has no edge.
"""

import math

import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyArrowPatch, Polygon, Rectangle

BG = "#1e212b"         # matches the other furnace figures
ENV = "#808080"        # byte 128: the half-radiance furnace
SPHERE = "#8a8a8a"     # a touch lighter so the reader can find it
RAY = "#f2b134"
LIGHT = "#c9ccd6"
TEXT = "#e6e8ee"
SUB = "#969baa"

fig, ax = plt.subplots(figsize=(8, 4.0), dpi=150)
fig.patch.set_facecolor(BG)
ax.set_xlim(0, 16)
ax.set_ylim(0, 8.2)
ax.set_aspect("equal")
ax.axis("off")

# The environment: the same radiance from every direction.
ax.add_patch(Rectangle((0.3, 0.3), 15.4, 7.0, facecolor=ENV, edgecolor="none"))
ax.text(0.3, 7.75, "Environment: radiance L from every direction",
        color=TEXT, fontsize=12)

# Light arriving at the sphere from all round it.
cx, cy, r = 10.0, 3.8, 1.9
for k in range(16):
    a = 2 * math.pi * k / 16
    x0, y0 = cx + (r + 1.35) * math.cos(a), cy + (r + 1.35) * math.sin(a)
    x1, y1 = cx + (r + 0.25) * math.cos(a), cy + (r + 0.25) * math.sin(a)
    if 0.6 < x0 < 15.4 and 0.6 < y0 < 7.0:
        ax.add_patch(FancyArrowPatch((x0, y0), (x1, y1), arrowstyle="-|>",
                                     mutation_scale=8, color=LIGHT, lw=0.8,
                                     alpha=0.7))

ax.add_patch(Circle((cx, cy), r, facecolor=SPHERE, edgecolor="#9a9a9a",
                    lw=1.0))
ax.text(cx, cy - 0.15, "albedo 1\nkeeps all\nof it", color="#2a2d36",
        fontsize=9.5, ha="center", va="center")

# The camera.
ex, ey = 1.6, 3.8
ax.add_patch(Polygon([(ex - 0.55, ey - 0.4), (ex - 0.55, ey + 0.4),
                      (ex + 0.15, ey)], closed=True, facecolor=TEXT))
ax.text(ex - 0.6, ey - 0.95, "camera", color=TEXT, fontsize=9.5)


def Ray(points, label=None, at=None):
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        ax.add_patch(FancyArrowPatch((x0, y0), (x1, y1), arrowstyle="-|>",
                                     mutation_scale=12, color=RAY, lw=1.8))
    if label:
        ax.text(*at, label, color=TEXT, fontsize=10.5, ha="left",
                va="center",
                bbox=dict(boxstyle="round,pad=0.3", fc=BG, ec="none"))


# Ray 1 misses: it reads the environment directly.
Ray([(ex + 0.2, ey + 0.15), (12.6, 6.6)],
    "miss:  pixel = L", (12.75, 6.6))

# Ray 2 hits, bounces in a random direction, and escapes.
hit_a = math.radians(160)
hx, hy = cx + r * math.cos(hit_a), cy + r * math.sin(hit_a)
bounce = (hx - 2.6, hy - 2.3)
Ray([(ex + 0.2, ey - 0.05), (hx, hy), bounce])
ax.text(bounce[0] - 1.2, bounce[1] - 0.5,
        "hit, bounce, escape:  pixel = 1 × L = L", color=TEXT,
        fontsize=10.5, va="center",
        bbox=dict(boxstyle="round,pad=0.3", fc=BG, ec="none"))

fig.subplots_adjust(left=0.02, right=0.98, top=0.98, bottom=0.02)
for ext in ("png", "svg"):
    fig.savefig(f"docs/images/furnace-diagram.{ext}", facecolor=BG)
print("wrote docs/images/furnace-diagram.png and .svg")
