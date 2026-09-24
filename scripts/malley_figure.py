"""The figure for posts/malleys-method.md: what a hexagonal aperture does to
the cosine lobe.

    python scripts/malley_figure.py                     # -> docs/images/malley-hexagon.png
    python scripts/malley_figure.py --out other.png --samples 500000

Left panel is the geometry: a regular hexagon inscribed in the unit disk, and
the six slivers between them that a bladed iris drops. Right panel is what that
costs after Malley's lift -- the disk lands on the exact cosine lobe, the
hexagon does not.

This reimplements the same rejection sampler rather than calling the renderer,
so the moments it prints are an independent check: they must agree with the ones
scripts/malley_figure.cc prints from the renderer's own Rng. The .cc remains the
source of the numbers quoted in the post.
"""

import argparse
import math

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.path import Path
from matplotlib.patches import PathPatch, Circle, Polygon

APOTHEM = math.sqrt(3.0) / 2.0  # 0.8660254, for a circumradius of 1

# Validated categorical slots 1 and 2, plus the chart's ink and surface.
BLUE, ORANGE = "#2a78d6", "#eb6834"
SURFACE = "#fcfcfb"
INK, SECONDARY, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS = "#e1e0d9", "#c3c2b7"


def in_hexagon(x, y):
    """The renderer's three half-plane tests, vectorised."""
    return (
        (np.abs(y) <= APOTHEM)
        & (np.abs(APOTHEM * x + 0.5 * y) <= APOTHEM)
        & (np.abs(APOTHEM * x - 0.5 * y) <= APOTHEM)
    )


def lift(x, y):
    """Malley's method: the height of the dome above (x, y)."""
    return np.sqrt(np.maximum(0.0, 1.0 - x * x - y * y))


def sample_lifted(n, shape, rng):
    """n values of cos(theta), rejection-sampled then lifted."""
    keep = []
    have = 0
    while have < n:
        x = rng.uniform(-1.0, 1.0, 1 << 20)
        y = rng.uniform(-1.0, 1.0, 1 << 20)
        m = (x * x + y * y <= 1.0) if shape == "disk" else in_hexagon(x, y)
        z = lift(x[m], y[m])
        keep.append(z)
        have += z.size
    return np.concatenate(keep)[:n]


def hexagon_vertices():
    a = np.arange(6) * (math.pi / 3.0)
    return np.column_stack([np.cos(a), np.sin(a)])


def draw_geometry(ax):
    """The unit disk, the hexagon inside it, and what falls between."""
    theta = np.linspace(0, 2 * math.pi, 400)
    circle = np.column_stack([np.cos(theta), np.sin(theta)])
    hexa = hexagon_vertices()

    # The slivers: the disk with the hexagon reversed punched out of it. Under
    # nonzero winding, the opposite orientation is what makes it a hole.
    ring = Path.make_compound_path(
        Path(circle, closed=True), Path(hexa[::-1], closed=True))
    ax.add_patch(PathPatch(ring, facecolor=ORANGE, alpha=0.22,
                           edgecolor="none", zorder=2))
    ax.add_patch(Polygon(hexa, closed=True, facecolor=SURFACE,
                         edgecolor=ORANGE, lw=2.0, zorder=3))
    ax.add_patch(Circle((0, 0), 1.0, facecolor="none", edgecolor=BLUE,
                        lw=2.0, zorder=4))
    # The hexagon's inscribed circle sits exactly at the apothem, so every
    # direction the hexagon drops lies outside it.
    ax.add_patch(Circle((0, 0), APOTHEM, facecolor="none", edgecolor=MUTED,
                        lw=1.1, ls=(0, (4, 3)), zorder=5))

    ax.annotate("corner still\nreaches r = 1", xy=(0.995, 0.02),
                xytext=(1.30, 0.48), fontsize=9, color=SECONDARY, ha="left",
                linespacing=1.45,
                arrowprops=dict(arrowstyle="-", color=AXIS, lw=1.0,
                                shrinkA=0, shrinkB=3))
    ax.annotate("edge stops at the\napothem, r = 0.866", xy=(0.30, APOTHEM),
                xytext=(0.10, 1.34), fontsize=9, color=SECONDARY, ha="left",
                linespacing=1.45,
                arrowprops=dict(arrowstyle="-", color=AXIS, lw=1.0,
                                shrinkA=0, shrinkB=3))
    ax.annotate("six slivers dropped —\nthe grazing directions",
                xy=(-0.87, -0.34), xytext=(-2.02, -1.16), fontsize=9,
                color=ORANGE, ha="left", linespacing=1.45,
                arrowprops=dict(arrowstyle="-", color=ORANGE, lw=1.0,
                                alpha=0.6, shrinkA=0, shrinkB=3))

    ax.set_xlim(-2.08, 1.98)
    ax.set_ylim(-1.58, 1.58)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("The aperture, from above", fontsize=11, color=INK, pad=10,
                 loc="left")


def draw_density(ax, disk, hexa, bins):
    edges = np.linspace(0.0, 1.0, bins + 1)
    width = edges[1] - edges[0]
    d_density = np.histogram(disk, bins=edges)[0] / (disk.size * width)
    h_density = np.histogram(hexa, bins=edges)[0] / (hexa.size * width)

    # The exact lobe sits underneath as a broad band, so the disk visibly
    # rides on it and the hexagon visibly leaves it.
    grid = np.linspace(0.0, 1.0, 200)
    ax.plot(grid, 2.0 * grid, color=MUTED, lw=5.0, alpha=0.30,
            solid_capstyle="round", zorder=2,
            label=r"exact cosine lobe,  $p(\cos\theta)=2\cos\theta$")
    ax.stairs(h_density, edges, color=ORANGE, lw=2.0, zorder=3,
              label="hexagonal aperture")
    ax.stairs(d_density, edges, color=BLUE, lw=2.0, zorder=4,
              label="disk aperture (what ships)")

    ax.axvline(0.5, color=AXIS, lw=1.0, ls=(0, (2, 3)), zorder=1)
    ax.annotate("the step at cos θ = 0.5\nis the apothem, lifted",
                xy=(0.5, 0.95), xytext=(0.075, 1.46), fontsize=9,
                color=SECONDARY, ha="left", linespacing=1.45,
                arrowprops=dict(arrowstyle="-", color=AXIS, lw=1.0,
                                shrinkA=3, shrinkB=3))

    # Direct labels at the curve ends, so identity never rests on colour.
    ax.text(1.015, h_density[-1], "hexagon", fontsize=9, color=ORANGE,
            va="center", ha="left")
    ax.text(1.015, d_density[-1], "disk", fontsize=9, color=BLUE,
            va="center", ha="left")

    # Where each lobe's mean cosine lands -- the row the post's table fails on.
    for value, colour, text in ((2 / 3, BLUE, "2/3"),
                                (hexa.mean(), ORANGE, "0.744")):
        ax.plot([value], [0], marker="^", ms=7, color=colour,
                clip_on=False, zorder=6)
        ax.text(value, -0.30, text, fontsize=8.5, color=colour, ha="center",
                va="top")
    ax.text(0.0, -0.30, "mean cos θ", fontsize=8.5, color=MUTED,
            ha="left", va="top")

    ax.set_xlabel("cos θ    (1 = straight up the normal,  0 = along the surface)",
                  fontsize=9.5, color=SECONDARY, labelpad=30)
    ax.set_ylabel("probability density", fontsize=9.5, color=SECONDARY,
                  labelpad=7)
    ax.set_xlim(0.0, 1.0)
    ax.set_ylim(0.0, 2.62)
    ax.set_xticks(np.arange(0, 1.01, 0.25))
    ax.set_yticks(np.arange(0, 2.51, 0.5))
    ax.tick_params(labelsize=9, colors=MUTED, length=0, pad=6)
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_color(SECONDARY)
    ax.grid(True, color=GRID, lw=0.8, zorder=0)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
        ax.spines[side].set_linewidth(1.0)

    ax.set_title("Where the bounces actually go", fontsize=11, color=INK,
                 pad=10, loc="left")
    leg = ax.legend(loc="upper left", fontsize=9, frameon=False,
                    handlelength=1.9, borderpad=0.1, labelspacing=0.45)
    for text in leg.get_texts():
        text.set_color(SECONDARY)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default="docs/images/malley-hexagon.png")
    ap.add_argument("--samples", type=int, default=2_000_000)
    ap.add_argument("--bins", type=int, default=40)
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    disk = sample_lifted(args.samples, "disk", rng)
    hexa = sample_lifted(args.samples, "hexagon", rng)

    # Cross-check against scripts/malley_figure.cc, which samples the same
    # shapes through the renderer's own Rng.
    print(f"{'':22}{'E[cos]':>10}{'E[cos^2]':>11}")
    print(f"{'disk lift':22}{disk.mean():10.5f}{(disk**2).mean():11.5f}")
    print(f"{'hexagon lift':22}{hexa.mean():10.5f}{(hexa**2).mean():11.5f}")
    print(f"{'exact (cosine lobe)':22}{2/3:10.5f}{0.5:11.5f}")
    past_60 = (hexa < 0.5).mean()
    print(f"\ndraws past 60 deg -- disk {(disk < 0.5).mean():.3f} "
          f"(exact 0.250), hexagon {past_60:.3f}")

    fig, (left, right) = plt.subplots(
        1, 2, figsize=(11.0, 4.7), dpi=120,
        gridspec_kw=dict(width_ratios=[1.0, 1.30], wspace=0.14))
    fig.patch.set_facecolor(SURFACE)
    for ax in (left, right):
        ax.set_facecolor(SURFACE)

    draw_geometry(left)
    draw_density(right, disk, hexa, args.bins)

    fig.subplots_adjust(left=0.012, right=0.935, top=0.88, bottom=0.205)
    fig.savefig(args.out, facecolor=SURFACE)
    print(f"\nwrote {args.out}")


if __name__ == "__main__":
    main()
