"""Draws docs/images/bvh/speed-times.png: the measured render time at every
step on the two Cornell box scenes.

    python scripts/speed_times_figure.py [--out PATH] [--csv PATH]

Each step is one bench session that timed an older and a newer configuration
on the same 300 x 300 image, alternating them by round. The figure shows both
medians of five rounds: the older as an open dot, the newer as a filled one,
joined by a stem. Times are per pass, one sample for every pixel (render ms /
spp), because the sessions ran at different spp: per pass, every point sits on
one axis.

Each session's medians come from that session alone. The same configuration
measured in two sessions can differ by a few percent as the laptop's clock
drifts, so the figure never joins one session's newer dot to the next
session's older one: that line would show the drift as a step's cost.

The plotted values are printed as Markdown, for the table beside the figure.
"""

import argparse
import statistics
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import NullLocator

import nw10_summary

# Each scene's validated categorical slot, fixed per scene so the scene keeps
# its colour in every figure, and its name in the CSV.
SCENES = {
    "cornell_blocks": ("#6250d6", "cornell_blocks_small"),
    "cornell_vehicles": ("#e34948", "cornell_vehicles_small"),
}
SURFACE = "#fcfcfb"
INK, SECONDARY, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS = "#e1e0d9", "#c3c2b7"

# (x label, older label and threads, newer label and threads, first CSV line
# of the blocks' session, of the vehicles' session).
STEPS = [
    ("1 → 20\nthreads", ("before-linear", "1"), ("before-linear", "20"),
     261, 291),
    ("mesh\nBVH", ("before-linear", "20"), ("before", "20"), 303, 601),
    ("SAH", ("before", "20"), ("before-sah", "20"), 313, 423),
    ("cull", ("before-sah", "20"), ("rung2-sah", "20"), 323, 433),
    ("identity\nskip", ("rung2-sah", "20"), ("rung2b-sah", "20"), 333, 443),
    ("top-level\ntree", ("rung2b-sah", "20"), ("rung3-sah", "20"), 343, 453),
    ("tree at\n16+ only", ("rung3-sah", "20"), ("rung3b-sah", "20"), 353,
     463),
    ("box\nprimitive", ("rung3b-sah", "20"), ("rung4-sah", "20"), 363, 473),
    ("small-node\nscan", ("rung4-sah", "20"), ("rung4b-sah", "20"), 373,
     483),
    ("all at\nonce", ("before-linear", "20"), ("rung4b-sah", "20"), 383, 611),
]
ALL_AT_ONCE = len(STEPS) - 1


def per_pass(rows, scene, lab, threads):
    """Median ms per pass and the spp, over one configuration's rows."""
    picked = [r for r in rows if r["scene"] == scene
              and nw10_summary.label(r) == lab and r["threads"] == threads]
    if not picked:
        sys.exit(f"no rows for {scene} {lab} at {threads} threads")
    spp = {r["spp"] for r in picked}
    if len(spp) != 1:
        sys.exit(f"{scene} {lab}: more than one spp in one session")
    spp = int(spp.pop())
    return statistics.median(float(r["render_ms"]) for r in picked) / spp, spp


def measure(path):
    """{scene: [(older ms, older spp, newer ms, newer spp)]}, one per step."""
    rows = nw10_summary.load(path)
    out = {}
    for k, (scene, (_, name)) in enumerate(SCENES.items()):
        out[scene] = []
        for _, older, newer, *first in STEPS:
            s = nw10_summary.session(rows, first[k])
            out[scene].append(per_pass(s, name, *older)
                              + per_pass(s, name, *newer))
    return out


def duration(ms):
    """A time in the unit a reader would say it in."""
    if ms >= 120_000:
        return f"{ms / 60_000:.1f} min"
    if ms >= 1000:
        return f"{ms / 1000:.3g} s"
    return f"{ms:.3g} ms"


def print_table(data):
    print("| Step | " + " | ".join(f"`{s}` older → newer, per pass"
                                    for s in SCENES) + " |")
    print("|---|" + "---|" * len(SCENES))
    for i, (name, *_) in enumerate(STEPS):
        cells = []
        for scene in SCENES:
            a, sa, b, sb = data[scene][i]
            spp = f"{sa} spp" if sa == sb else f"{sa} → {sb} spp"
            cells.append(f"{duration(a)} → {duration(b)} ({spp})")
        print(f"| {name.replace(chr(10), ' ')} | " + " | ".join(cells) + " |")


LINE = 1.9
MARKER = 7.0
DX = 0.13  # the two scenes sit either side of each step, not on each other


def draw(data, out):
    plt.rcParams.update({"font.family": ["Segoe UI", "DejaVu Sans"],
                         "font.size": 11})
    fig, ax = plt.subplots(figsize=(10.0, 6.6), dpi=160)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    fig.subplots_adjust(left=0.085, right=0.985, top=0.80, bottom=0.2)

    ax.set_yscale("log")
    ax.set_ylim(3, 150_000)
    ticks = [10, 100, 1000, 10_000, 60_000]
    ax.set_yticks(ticks)
    ax.set_yticklabels(["10 ms", "100 ms", "1 s", "10 s", "1 min"])
    ax.yaxis.set_minor_locator(NullLocator())
    ax.set_xlim(-0.6, len(STEPS) - 0.2)
    ax.set_xticks(range(len(STEPS)))
    ax.set_xticklabels([s[0] for s in STEPS], fontsize=9.5, color=SECONDARY)
    ax.tick_params(length=0, pad=6)
    ax.tick_params(axis="y", labelcolor=MUTED, labelsize=10)
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.yaxis.grid(True, color=GRID, lw=0.8)
    ax.set_axisbelow(True)

    # The last step is the whole ladder in one session, set apart.
    ax.axvline(ALL_AT_ONCE - 0.5, color=GRID, lw=0.8, zorder=0)

    for k, (scene, (color, _)) in enumerate(SCENES.items()):
        dx = DX * (2 * k - 1)
        for i, (a, _, b, _) in enumerate(data[scene]):
            x = i + dx
            ax.plot([x, x], [a, b], color=color, lw=LINE, zorder=3,
                    solid_capstyle="butt")
            ax.plot(x, a, marker="o", ms=MARKER, mfc=SURFACE, mec=color,
                    mew=1.8, zorder=4)
            ax.plot(x, b, marker="o", ms=MARKER, mfc=color, mec=SURFACE,
                    mew=1.6, zorder=5)

    # Labels on the first step's older time and the last step's newer time:
    # where each scene starts and where it ends. Each sits right of its dot;
    # the other scene's stem at that step spans other times, so they clear.
    for k, scene in enumerate(SCENES):
        dx = DX * (2 * k - 1)
        first, last = data[scene][0], data[scene][ALL_AT_ONCE]
        for x, ms in ((dx, first[0]), (ALL_AT_ONCE + dx, last[2])):
            ax.text(x + 0.14, ms, duration(ms), ha="left", va="center",
                    color=INK, fontsize=10)

    fig.text(0.085, 0.965, "The measured time at every step",
             color=INK, fontsize=14, weight="semibold", va="top")
    fig.text(0.085, 0.922,
             "Time for one pass, one sample for every pixel of the 300 × 300 "
             "image, log scale. Each step is one session: medians of 5 "
             "rounds.",
             color=SECONDARY, fontsize=10.5, va="top")

    style = dict(loc="upper left", frameon=False, fontsize=10,
                 handlelength=2.0, columnspacing=1.8, labelcolor=SECONDARY)
    handles = [Line2D([], [], color=SCENES[s][0], lw=LINE, marker="o",
                      ms=MARKER, mfc=SCENES[s][0], mec=SURFACE)
               for s in SCENES]
    handles += [Line2D([], [], lw=0, marker="o", ms=MARKER, mfc=SURFACE,
                       mec=MUTED, mew=1.8),
                Line2D([], [], lw=0, marker="o", ms=MARKER, mfc=MUTED,
                       mec=SURFACE)]
    fig.legend(handles, list(SCENES) + ["the step's older build",
                                        "its newer build"],
               ncol=4, bbox_to_anchor=(0.078, 0.892), **style)

    fig.text(0.085, 0.035,
             "Render ms ÷ samples per pixel. The blocks ran at 64 spp; the "
             "vehicles at 4 spp for threads, 16 spp after, and 1 spp for the "
             "unoptimised side,\nwhich at 16 spp outlasts the laptop's turbo "
             "power. 20 threads from the mesh BVH on. Two sessions can time "
             "one build a few percent apart.",
             color=MUTED, fontsize=9.5, va="bottom", linespacing=1.4)

    fig.savefig(out, facecolor=SURFACE)
    print(f"\nwrote {out}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="docs/images/bvh/speed-times.png")
    parser.add_argument("--csv", default="docs/data/nw10-speed.csv")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")  # the table prints →
    data = measure(args.csv)
    print_table(data)
    draw(data, args.out)


if __name__ == "__main__":
    main()
