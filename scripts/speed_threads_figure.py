"""Draws docs/images/bvh/speed-threads.png: speed-up against thread count.

    python scripts/speed_threads_figure.py [--out PATH] [--csv PATH]

One line per scene: the final build at each thread count, against ideal
scaling, where n threads run n times faster. Each count ran in its own bench
session against 20 threads, alternating the two by round, so a count's ratio
to 20 threads is the geometric mean of its per-round ratios. The speed-up at
n threads is then the 1-thread ratio divided by the n-thread ratio: two
sessions, marked "≈" in the table. At 20 threads it is the 1-thread ratio
itself.

The image changes with the thread count (each thread seeds its own
generator), so these runs carry no image check. The plotted values are
printed as Markdown.
"""

import argparse
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

import nw10_summary

# Each scene's validated categorical slot, fixed per scene so the scene keeps
# its colour in every figure; its name in the CSV; and the first CSV line of
# each count's session against 20 threads.
SCENES = {
    "cornell_blocks": ("#6250d6", "cornell_blocks_small",
                       {1: 523, 2: 529, 4: 535, 8: 541, 12: 547, 14: 553}),
    "cornell_vehicles": ("#e34948", "cornell_vehicles_small",
                         {1: 559, 2: 565, 4: 571, 8: 577, 12: 583, 14: 589}),
}
LABEL = "rung4b"
SURFACE = "#fcfcfb"
INK, SECONDARY, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS = "#e1e0d9", "#c3c2b7"

MOST = 20  # hardware threads on this machine

LINE = 1.9
MARKER = 7.0


def ratio_to_most(rows, name, count):
    """n-thread ms / 20-thread ms matched by round: geometric mean, rounds,
    and the scene's spp."""
    ms = {}
    for r in rows:
        if r["scene"] == name and nw10_summary.label(r) == LABEL:
            key = (int(r["threads"]), int(r["round"]))
            if key in ms:
                sys.exit(f"{name}: two rows for {key}")
            ms[key] = float(r["render_ms"])
    rounds = sorted(k for n, k in ms if n == count and (MOST, k) in ms)
    if not rounds:
        sys.exit(f"{name}: no rounds pair {count} threads with {MOST}")
    spp = {r["spp"] for r in rows if r["scene"] == name}
    if len(spp) != 1:
        sys.exit(f"{name}: the session ran at more than one spp")
    return (nw10_summary.geomean(ms[(count, k)] / ms[(MOST, k)]
                                 for k in rounds), len(rounds), spp.pop())


def sweep(path):
    """{scene: (spp, [(threads, speed-up against 1 thread, rounds)])}."""
    rows = nw10_summary.load(path)
    out = {}
    for scene, (_, name, sessions) in SCENES.items():
        ratios = {n: ratio_to_most(nw10_summary.session(rows, first), name, n)
                  for n, first in sessions.items()}
        if len({spp for _, _, spp in ratios.values()}) != 1:
            sys.exit(f"{scene}: the counts ran at different spp")
        one = ratios[1][0]
        points = [(n, one / r, k) for n, (r, k, _) in sorted(ratios.items())]
        points.append((MOST, one, ratios[1][1]))
        out[scene] = (ratios[1][2], points)
    return out


def print_table(data):
    print("| Scene | spp | Threads | rounds | vs 1 thread | of ideal |")
    print("|---|---|---|---|---|---|")
    for scene, (spp, points) in data.items():
        for n, s, k in points:
            approx = "≈" if n not in (1, MOST) else ""
            print(f"| `{scene}` | {spp} | {n} | {k} | {approx}{s:.2f}× | "
                  f"{s / n:.0%} |")


def draw(data, out):
    plt.rcParams.update({"font.family": ["Segoe UI", "DejaVu Sans"],
                         "font.size": 11})
    fig, ax = plt.subplots(figsize=(8.6, 6.0), dpi=160)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    fig.subplots_adjust(left=0.085, right=0.8, top=0.79, bottom=0.11)

    ax.set_xlim(0, MOST + 0.6)
    ax.set_ylim(0, MOST + 0.6)
    counts = sorted({n for _, pts in data.values() for n, _, _ in pts})
    ax.set_xticks(counts)
    ax.set_yticks([1, 5, 10, 15, 20])
    ax.set_yticklabels(["1×", "5×", "10×", "15×", "20×"])
    ax.tick_params(length=0, pad=6, labelsize=10)
    ax.tick_params(axis="x", labelcolor=SECONDARY)
    ax.tick_params(axis="y", labelcolor=MUTED)
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.yaxis.grid(True, color=GRID, lw=0.8)
    ax.set_axisbelow(True)
    ax.axhline(0, color=AXIS, lw=1.1)
    ax.set_xlabel("threads", color=SECONDARY, fontsize=10.5, labelpad=8)

    # Ideal: n threads, n times faster.
    ax.plot([1, MOST], [1, MOST], color=MUTED, lw=1.3, ls=(0, (4, 3)),
            zorder=2)
    ax.text(MOST + 0.5, MOST, f"ideal  {MOST}×", ha="left", va="center",
            color=SECONDARY, fontsize=10.5, clip_on=False)

    ends = []
    for scene, (_, points) in data.items():
        color = SCENES[scene][0]
        xs = [n for n, _, _ in points]
        ys = [s for _, s, _ in points]
        ax.plot(xs, ys, color=color, lw=LINE, solid_joinstyle="round",
                solid_capstyle="round", marker="o", ms=MARKER, mfc=color,
                mec=SURFACE, mew=1.6, zorder=3)
        ends.append((ys[-1], scene))

    # Two end values within a label's height split apart, the higher just
    # above its point and the lower just below.
    ends.sort(reverse=True)
    close = len(ends) == 2 and ends[0][0] - ends[1][0] < 0.8
    for i, (y, scene) in enumerate(ends):
        va, dy = (("bottom", 0.05) if i == 0 else ("top", -0.05)) if close \
            else ("center", 0)
        ax.text(MOST + 0.5, y + dy, f"{scene}  {y:.2f}×", ha="left", va=va,
                color=INK, fontsize=10.5, clip_on=False)

    fig.text(0.085, 0.965, "Threads on the two Cornell boxes",
             color=INK, fontsize=14, weight="semibold", va="top")
    fig.text(0.085, 0.918,
             "The final build at each thread count, against its own time on "
             "one thread.\ni7-12700H: 6 performance cores with two threads "
             "each, plus 8 efficiency cores.",
             color=SECONDARY, fontsize=10.5, va="top", linespacing=1.4)
    handles = [Line2D([], [], color=SCENES[s][0], lw=LINE, marker="o",
                      ms=MARKER, mfc=SCENES[s][0], mec=SURFACE)
               for s in SCENES]
    handles.append(Line2D([], [], color=MUTED, lw=1.3, ls=(0, (4, 3))))
    labels = [f"{s} ({data[s][0]} spp)" for s in SCENES]
    labels.append("ideal: n threads, n× faster")
    fig.legend(handles, labels, loc="upper left", ncol=3, frameon=False,
               bbox_to_anchor=(0.078, 0.845), fontsize=10, handlelength=2.0,
               columnspacing=1.8, labelcolor=SECONDARY)

    fig.savefig(out, facecolor=SURFACE)
    print(f"\nwrote {out}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="docs/images/bvh/speed-threads.png")
    parser.add_argument("--csv", default="docs/data/nw10-speed.csv")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")  # the table prints × and ≈
    data = sweep(args.csv)
    print_table(data)
    draw(data, args.out)


if __name__ == "__main__":
    main()
