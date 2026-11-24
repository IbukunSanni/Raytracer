"""Draws docs/images/bvh/speed-overlay.png: the two Cornell box scenes at
every step, from one thread with no optimisations to all of them.

    python scripts/speed_overlay_figure.py [--out PATH] [--csv PATH]

One line per scene, on a log axis, against that scene's own start: 20
threads, no optimisations, at 1x. The first step is the same scene on one
thread, so every later step is measured from the 20-thread start.

  line    each step timed against the step before it, in its own bench
          session, alternating the two by round; the step's gain is the
          geometric mean of the per-round ratios, and the line multiplies
          the gains along. A point several steps in carries the error of
          several sessions, marked "≈" in the table.
  ring    the last step timed directly against the start, in one session;
          its bar runs from the slowest round's ratio to the fastest's.
          Where the ring and the line disagree, the ring is the better
          number.

Every ratio is of time per sample (render ms / spp), so a session may time
its two sides at different spp. The vehicles' unoptimised side does: at the
spp of the rest of the ladder one frame takes about five minutes on 20
threads, longer than the laptop holds its turbo power, and the clock it
falls to would inflate the ratio. At 1 spp the frame takes seconds, as the
other side's does.

SAH is a switch on the mesh BVH, off by default; from its step on, both
builds in every session run with it on, so the last step is every
optimisation, SAH included. The blocks have no triangle meshes, so the two
mesh steps can only measure noise on them.

The plotted values are printed as Markdown, for the table beside the figure.
"""

import argparse
import collections
import math
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import NullLocator

import nw10_summary

# Each scene's validated categorical slot, its name in the CSV, its samples
# per pixel through the ladder's optimised sides, and the first CSV line of
# its 1-thread against 20-thread session, which ran before the ladder.
SCENES = {
    "cornell_blocks": ("#6250d6", "cornell_blocks_small", 64, 261),
    "cornell_vehicles": ("#e34948", "cornell_vehicles_small", 16, 291),
}
SURFACE = "#fcfcfb"
INK, SECONDARY, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS = "#e1e0d9", "#c3c2b7"

STEPS = ["1\nthread", "20\nthreads", "mesh\nBVH", "SAH", "cull",
         "identity\nskip", "top-level\ntree", "tree at\n16+ only",
         "box\nprimitive", "small-node\nscan"]
ONE_THREAD, START, LAST = 0, 1, len(STEPS) - 1
NONE_LABEL = "before-linear"

# (step, older label, newer label); the first CSV line of each scene's
# session for that step follows, blocks then vehicles.
LADDER = [
    (2, "before-linear", "before", 303, 601),
    (3, "before", "before-sah", 313, 423),
    (4, "before-sah", "rung2-sah", 323, 433),
    (5, "rung2-sah", "rung2b-sah", 333, 443),
    (6, "rung2b-sah", "rung3-sah", 343, 453),
    (7, "rung3-sah", "rung3b-sah", 353, 463),
    (8, "rung3b-sah", "rung4-sah", 363, 473),
    (9, "rung4-sah", "rung4b-sah", 373, 483),
]
# The last step against the start, in one session: blocks, then vehicles.
DIRECT = ("before-linear", "rung4b-sah", 383, 611)

Speedup = collections.namedtuple("Speedup", "value low high rounds")
# x is the step; sessions counts the measurements multiplied into value.
Point = collections.namedtuple("Point", "x value sessions step")


def by_round(rows, pick):
    """ms per sample by round over the rows pick accepts, one row per
    round."""
    ms = {}
    for r in rows:
        if pick(r):
            k = int(r["round"])
            if k in ms:
                sys.exit(f"two rows for round {k}: the pick matches more than "
                         "one configuration")
            ms[k] = float(r["render_ms"]) / int(r["spp"])
    if not ms:
        sys.exit("no rows match the pick")
    return ms


def speedup(old, new):
    """old / new matched by round: geometric mean, smallest, largest."""
    rounds = sorted(old.keys() & new.keys())
    if not rounds:
        sys.exit("the two configurations share no rounds")
    ratios = [old[k] / new[k] for k in rounds]
    return Speedup(nw10_summary.geomean(ratios), min(ratios), max(ratios),
                   len(rounds))


def pick(scene, lab, threads="20"):
    return lambda r: (r["scene"] == scene and nw10_summary.label(r) == lab
                      and r["threads"] == threads)


def ladder(path):
    """Each scene's points and its direct ring at the last step."""
    rows = nw10_summary.load(path)
    lines, rings = {}, {}
    for k, (scene, (_, name, _, threads_line)) in enumerate(SCENES.items()):
        s = nw10_summary.session(rows, threads_line)
        t = speedup(by_round(s, pick(name, NONE_LABEL, "1")),
                    by_round(s, pick(name, NONE_LABEL, "20")))
        points = [Point(ONE_THREAD, 1 / t.value, 1,
                        Speedup(t.value, t.low, t.high, t.rounds)),
                  Point(START, 1.0, 0, None)]
        total = 1.0
        for sessions, (step, old, new, *first) in enumerate(LADDER, 1):
            s = nw10_summary.session(rows, first[k])
            g = speedup(by_round(s, pick(name, old)),
                        by_round(s, pick(name, new)))
            total *= g.value
            points.append(Point(step, total, sessions, g))
        lines[scene] = points
        old, new, *first = DIRECT
        s = nw10_summary.session(rows, first[k])
        rings[scene] = speedup(by_round(s, pick(name, old)),
                               by_round(s, pick(name, new)))
    return lines, rings


def times(v, approx=False):
    return f"{'≈' if approx else ''}{v:,.3g}×"


def print_table(lines, rings):
    print("| Step | " + " | ".join(f"`{s}` this step | `{s}` total"
                                    for s in SCENES) + " |")
    print("|---|" + "---|---|" * len(SCENES))
    for step, name in enumerate(STEPS):
        cells = []
        for scene in SCENES:
            p = lines[scene][step]
            # The threads gain belongs on the 20-threads row it leads to.
            g = lines[scene][ONE_THREAD].step if step == START else p.step
            this = "" if step == ONE_THREAD else times(g.value)
            cells += [this, times(p.value, p.sessions > 1)]
        print(f"| {name.replace(chr(10), ' ')} | " + " | ".join(cells) + " |")
    cells = []
    for scene in SCENES:
        cells += ["", times(rings[scene].value)]
    print("| last step, timed directly against the start | "
          + " | ".join(cells) + " |")


LINE = 1.9      # points; about 2 px once the post scales the image to half
MARKER = 7.0
RING_DX = 0.17  # rings sit just right of the line's point, not on it


def draw(lines, rings, out):
    plt.rcParams.update({"font.family": ["Segoe UI", "DejaVu Sans"],
                         "font.size": 11})
    fig, ax = plt.subplots(figsize=(10.0, 6.6), dpi=160)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    fig.subplots_adjust(left=0.075, right=0.985, top=0.80, bottom=0.2)

    top = max(max(g.high for g in rings.values()),
              max(p.value for pts in lines.values() for p in pts))
    ax.set_yscale("log")
    ax.set_ylim(0.06, top * 3.2)
    ax.set_xlim(-0.35, LAST + 2.3)
    ticks = [10.0 ** e for e in range(-1, int(math.log10(top)) + 1)]
    ax.set_yticks(ticks)
    ax.set_yticklabels([f"{t:g}×" for t in ticks])
    ax.yaxis.set_minor_locator(NullLocator())
    ax.set_xticks(range(len(STEPS)))
    ax.set_xticklabels(STEPS, fontsize=9.5, color=SECONDARY)
    ax.tick_params(length=0, pad=6)
    ax.tick_params(axis="y", labelcolor=MUTED, labelsize=10)
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.yaxis.grid(True, color=GRID, lw=0.8)
    ax.set_axisbelow(True)
    ax.axhline(1, color=AXIS, lw=1.1, zorder=1)

    # The three stages of the work, divided by hairlines.
    for x in (1.5, 3.5):
        ax.axvline(x, color=GRID, lw=0.8, zorder=0)
    for x, text in ((0.5, "threads"), (2.5, "inside each mesh"),
                    (6.5, "across the scene")):
        ax.text(x, 0.985, text, ha="center", va="top", color=SECONDARY,
                fontsize=10.5, transform=ax.get_xaxis_transform())
    # The two 1-thread points nearly coincide, so the threads stage names
    # both gains under its header, clear of the lines.
    gains = "\n".join(f"{s.split('_')[1]} {lines[s][0].step.value:.2f}×"
                      for s in SCENES)
    ax.text(0.575, 0.93, f"20 threads\nagainst one:\n{gains}", ha="center",
            va="top", color=MUTED, fontsize=9.5, linespacing=1.4,
            transform=ax.get_xaxis_transform())

    ring_x = LAST + RING_DX
    for scene, points in lines.items():
        color = SCENES[scene][0]
        ax.plot([p.x for p in points], [p.value for p in points],
                color=color, lw=LINE, solid_joinstyle="round",
                solid_capstyle="round", zorder=3)
        shown = [p for p in points if p.sessions > 0]
        ax.plot([p.x for p in shown], [p.value for p in shown], ls="none",
                marker="o", ms=MARKER, mfc=color, mec=SURFACE, mew=1.6,
                zorder=4)
        g = rings[scene]
        ax.plot([ring_x, ring_x], [g.low, g.high], color=color, lw=1.3,
                zorder=3)
        ax.plot(ring_x, g.value, marker="o", ms=MARKER + 1, mfc=SURFACE,
                mec=color, mew=1.8, zorder=4)
        ax.text(ring_x + 0.2, g.value, f"{scene}  {times(g.value)}",
                ha="left", va="center", color=INK, fontsize=10.5)
    ax.plot(START, 1, marker="o", ms=MARKER, mfc=SECONDARY, mec=SURFACE,
            mew=1.6, zorder=5)
    ax.text(START - 0.12, 1.45, "the new start", ha="right", va="bottom",
            color=SECONDARY, fontsize=10)

    fig.text(0.075, 0.965, "Every speed-up on the two Cornell boxes",
             color=INK, fontsize=14, weight="semibold", va="top")
    fig.text(0.075, 0.922,
             "Against each scene's own start at 20 threads with no "
             "optimisations, log scale. A line multiplies each step's gain "
             "over the step before.",
             color=SECONDARY, fontsize=10.5, va="top")

    style = dict(loc="upper left", frameon=False, fontsize=10,
                 handlelength=2.0, columnspacing=1.8, labelcolor=SECONDARY)
    handles = [Line2D([], [], color=SCENES[s][0], lw=LINE, marker="o",
                      ms=MARKER, mfc=SCENES[s][0], mec=SURFACE)
               for s in SCENES]
    handles.append(Line2D([], [], color=MUTED, lw=1.3, marker="o",
                          ms=MARKER + 1, mfc=SURFACE, mec=MUTED, mew=1.8))
    fig.legend(handles, list(SCENES) + [
        "last step timed directly against the start (bar: range of rounds)"],
               ncol=3, bbox_to_anchor=(0.068, 0.892), **style)

    spp = {s: SCENES[s][2] for s in SCENES}
    fig.text(0.075, 0.035,
             f"300 × 300, time per sample. The blocks at "
             f"{spp['cornell_blocks']} spp; the vehicles at 4 spp for "
             f"threads, then {spp['cornell_vehicles']} spp, with the "
             f"unoptimised side at 1 spp.\nFrom SAH on, both builds in "
             f"every step run with SAH, so the last step is every "
             f"optimisation. The blocks have no meshes.",
             color=MUTED, fontsize=9.5, va="bottom", linespacing=1.4)

    fig.savefig(out, facecolor=SURFACE)
    print(f"\nwrote {out}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="docs/images/bvh/speed-overlay.png")
    parser.add_argument("--csv", default="docs/data/nw10-speed.csv")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")  # the table prints × and ≈
    lines, rings = ladder(args.csv)
    print_table(lines, rings)
    draw(lines, rings, args.out)


if __name__ == "__main__":
    main()
