"""Turns the scene-tree bench CSV into the tables the speed post quotes.

    python scripts/nw10_summary.py [--csv PATH]

The CSV is written by scripts/bench_ab.sh, one row per run, in the order the
runs happened. A label such as "before" recurs across bench sessions, so rows
can only be grouped into sessions by where they sit in the file; SESSIONS
lists each session's first and last line (1-based, the header is line 1).
The file is append-only, so those line numbers stay valid.

  paired     two labels timed on the same scene, spp and thread count in one
             session, matched by round. speedup is the geometric mean of the
             per-round ratios A ms / B ms, where A is the label that ran
             first in round 1; min-max are the smallest and largest per-round
             ratios. "B faster" counts the rounds where B took less time.
  image      "same" when every hash on both sides is one value; rows with no
             hash (thread-count runs, where the image depends on the thread
             count) print "-".
  threads    one label on one scene at several thread counts in one session:
             the median ms at each count, and 1-thread median / that median.
  end to end the first label on 1 thread against the second on the most
             threads, matched by round: threads and the ladder together.
  medians    each label's median ms; the median shrugs off the rounds where
             the laptop throttled.

Output is Markdown.
"""

import argparse
import collections
import csv
import math
import statistics
import sys

SESSIONS = [
    (2, 11, "before alone, 1 Nov"),
    (12, 15, "before at 2 spp, 1 thread and 20, 1 Nov (one run each)"),
    (16, 25, "before alone, quiet machine, 1 Nov"),
    (26, 45, "rung 2 (cull) against before"),
    (46, 65, "rung 2b (identity skip) against rung 2"),
    (66, 85, "rung 3 (top-level tree) against rung 2b"),
    (86, 105, "rung 3b (tree only at 16+ children) against rung 3"),
    (106, 125, "rung 3b against rung 2b, direct"),
    (126, 145, "rung 4 (box primitive) against rung 3b"),
    (146, 165, "rung 4 against before, direct"),
    (166, 185, "rung 4b (small-node scan) against rung 4"),
    (186, 205, "rung 4b against before, direct"),
    (206, 207, "the book's frame, 1,000 spp"),
    (208, 211, "the book's frame at 2 spp, rung 4b against before"),
    (212, 227, "before and rung 4b at 1 thread and 20"),
    (228, 240, "rung 4b, thread sweep"),
    (241, 250, "before on simple, 1 thread and 20, the scene's own 1 spp"),
    (251, 260, "before on simple, 1 thread and 20, 256 spp"),
    (261, 270, "cubes, small, no optimisations (before, linear), 1 thread "
               "and 20"),
    (271, 280, "cubes, small, 20 threads, no optimisations against rung 4b"),
    (281, 290, "vehicles, small, 20 threads, no optimisations against "
               "rung 4b"),
    (291, 300, "vehicles, small, no optimisations (before, linear), "
               "1 thread and 20"),
    (301, 302, "the big images on the current build, RT_MIXTURE=1, one run "
               "each"),
    (303, 312, "blocks, small, 64 spp, 20 threads: mesh BVH (linear vs "
               "iterative median, before)"),
    (313, 322, "blocks, small, 64 spp, 20 threads: SAH (median vs sah, "
               "before)"),
    (323, 332, "blocks, small, 64 spp, 20 threads: cull (before vs "
               "rung2, sah)"),
    (333, 342, "blocks, small, 64 spp, 20 threads: identity skip (rung2 "
               "vs rung2b, sah)"),
    (343, 352, "blocks, small, 64 spp, 20 threads: top-level tree "
               "(rung2b vs rung3, sah)"),
    (353, 362, "blocks, small, 64 spp, 20 threads: tree at 16+ (rung3 vs "
               "rung3b, sah)"),
    (363, 372, "blocks, small, 64 spp, 20 threads: box primitive (rung3b "
               "vs rung4, sah)"),
    (373, 382, "blocks, small, 64 spp, 20 threads: small-node scan "
               "(rung4 vs rung4b, sah)"),
    (383, 392, "blocks, small, 64 spp, 20 threads: direct, none (before "
               "linear) vs all (rung4b sah)"),
    (393, 402, "blocks, small, 64 spp, 20 threads: recursive vs "
               "iterative, median (rung4b)"),
    (403, 412, "blocks, small, 64 spp, 20 threads: recursive vs "
               "iterative, sah (rung4b)"),
    (413, 422, "vehicles, small, 16 spp, 20 threads: mesh BVH (linear vs "
               "iterative median, before); the unoptimised runs (~300 s) "
               "outlast the turbo window, so the ratio is inflated: use "
               "lines 601-620"),
    (423, 432, "vehicles, small, 16 spp, 20 threads: SAH (median vs sah, "
               "before)"),
    (433, 442, "vehicles, small, 16 spp, 20 threads: cull (before vs "
               "rung2, sah)"),
    (443, 452, "vehicles, small, 16 spp, 20 threads: identity skip "
               "(rung2 vs rung2b, sah)"),
    (453, 462, "vehicles, small, 16 spp, 20 threads: top-level tree "
               "(rung2b vs rung3, sah)"),
    (463, 472, "vehicles, small, 16 spp, 20 threads: tree at 16+ (rung3 "
               "vs rung3b, sah)"),
    (473, 482, "vehicles, small, 16 spp, 20 threads: box primitive "
               "(rung3b vs rung4, sah)"),
    (483, 492, "vehicles, small, 16 spp, 20 threads: small-node scan "
               "(rung4 vs rung4b, sah)"),
    (493, 502, "vehicles, small, 16 spp, 20 threads: direct, none "
               "(before linear) vs all (rung4b sah); the unoptimised "
               "runs (~300 s) outlast the turbo window, so the ratio is "
               "inflated: use lines 601-620"),
    (503, 512, "vehicles, small, 16 spp, 20 threads: recursive vs "
               "iterative, median (rung4b)"),
    (513, 522, "vehicles, small, 16 spp, 20 threads: recursive vs "
               "iterative, sah (rung4b)"),
    (523, 528, "blocks, small, 64 spp: rung4b, 1 threads against 20"),
    (529, 534, "blocks, small, 64 spp: rung4b, 2 threads against 20"),
    (535, 540, "blocks, small, 64 spp: rung4b, 4 threads against 20"),
    (541, 546, "blocks, small, 64 spp: rung4b, 8 threads against 20"),
    (547, 552, "blocks, small, 64 spp: rung4b, 12 threads against 20"),
    (553, 558, "blocks, small, 64 spp: rung4b, 14 threads against 20"),
    (559, 564, "vehicles, small, 16 spp: rung4b, 1 threads against 20"),
    (565, 570, "vehicles, small, 16 spp: rung4b, 2 threads against 20"),
    (571, 576, "vehicles, small, 16 spp: rung4b, 4 threads against 20"),
    (577, 582, "vehicles, small, 16 spp: rung4b, 8 threads against 20"),
    (583, 588, "vehicles, small, 16 spp: rung4b, 12 threads against 20"),
    (589, 594, "vehicles, small, 16 spp: rung4b, 14 threads against 20"),
    (595, 600, "counts, vehicles 1 spp, RT_STATS=1"),
    (601, 610, "vehicles, small, 20 threads: mesh BVH, linear at 1 spp "
               "against before at 16 spp; compare ms per sample"),
    (611, 620, "vehicles, small, 20 threads: direct, linear at 1 spp "
               "against rung4b sah at 16 spp; compare ms per sample"),
    (621, 626, "counts, RT_STATS=1: blocks 64 spp, vehicles 16 spp, for "
               "rays per frame"),
]


def label(row):
    """The bench label without the commit prefix bench_ab.sh sometimes adds."""
    return row["commit"].split(":")[-1]


def load(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    for i, r in enumerate(rows):
        r["line"] = i + 2
    return rows


def session(rows, first_line):
    """The rows of the session in SESSIONS that starts at first_line."""
    for first, last, _ in SESSIONS:
        if first == first_line:
            return [r for r in rows if first <= r["line"] <= last]
    sys.exit(f"no bench session starts at line {first_line}")


def geomean(values):
    return math.exp(statistics.fmean(math.log(v) for v in values))


def table(header, lines):
    if not lines:
        return
    print("| " + " | ".join(header) + " |")
    print("|" + "---|" * len(header))
    for line in lines:
        print("| " + " | ".join(line) + " |")
    print()


def config(row):
    return (row["scene"], row["spp"], row["threads"])


def paired(rows):
    """Every scene, spp and thread count that two labels share."""
    by_config = collections.defaultdict(list)
    for r in rows:
        by_config[config(r)].append(r)
    lines = []
    for (scene, spp, threads), group in by_config.items():
        labels = list(dict.fromkeys(label(r) for r in group))
        if len(labels) != 2:
            continue
        a, b = labels
        ms = {(label(r), int(r["round"])): float(r["render_ms"])
              for r in group}
        rounds = sorted(k for (lab, k) in ms if lab == a and (b, k) in ms)
        ratios = [ms[(a, k)] / ms[(b, k)] for k in rounds]
        hashes = {r["image_md5"] for r in group}
        image = ("-" if "" in hashes else
                 "same" if len(hashes) == 1 else "differ")
        median_a = statistics.median(ms[(a, k)] for k in rounds)
        median_b = statistics.median(ms[(b, k)] for k in rounds)
        lines.append([
            scene, f"{spp} spp, {threads} t", a, b, str(len(rounds)),
            f"{median_a:,.0f}", f"{median_b:,.0f}",
            f"**{geomean(ratios):.3g}x**",
            f"{min(ratios):.3g}-{max(ratios):.3g}",
            f"{sum(x > 1 for x in ratios)}/{len(rounds)}", image])
    return lines


def threads(rows):
    """Each label and scene timed at more than one thread count."""
    groups = collections.defaultdict(lambda: collections.defaultdict(list))
    for r in rows:
        groups[(label(r), r["scene"], r["spp"])][int(r["threads"])].append(
            float(r["render_ms"]))
    lines = []
    for (lab, scene, spp), by_count in groups.items():
        if len(by_count) < 2 or 1 not in by_count:
            continue
        one = statistics.median(by_count[1])
        for count in sorted(by_count):
            m = statistics.median(by_count[count])
            lines.append([lab, scene, spp, str(count),
                          str(len(by_count[count])), f"{m:,.0f}",
                          f"{one / m:.2f}x"])
    return lines


def end_to_end(rows):
    """A on 1 thread against B on the most threads, matched by round.

    This is the whole gain a reader sees, threads and less work per ray
    together, so it is only computed where both labels ran both counts.
    """
    groups = collections.defaultdict(dict)
    for r in rows:
        groups[(r["scene"], r["spp"])][(label(r), int(r["threads"]),
                                        int(r["round"]))] = float(
                                            r["render_ms"])
    lines = []
    for (scene, spp), ms in groups.items():
        labels = list(dict.fromkeys(lab for lab, _, _ in ms))
        counts = sorted({t for _, t, _ in ms})
        if len(labels) != 2 or len(counts) < 2 or counts[0] != 1:
            continue
        a, b = labels
        top = counts[-1]
        rounds = sorted(k for lab, t, k in ms
                        if lab == a and t == 1 and (b, top, k) in ms)
        if not rounds:
            continue
        ratios = [ms[(a, 1, k)] / ms[(b, top, k)] for k in rounds]
        lines.append([scene, spp, f"{a}, 1 t", f"{b}, {top} t",
                      str(len(rounds)), f"**{geomean(ratios):.3g}x**",
                      f"{min(ratios):.3g}-{max(ratios):.3g}"])
    return lines


def medians(rows):
    groups = collections.defaultdict(list)
    for r in rows:
        groups[(label(r), config(r))].append(float(r["render_ms"]))
    return [[lab, scene, f"{spp} spp, {t} t", str(len(ms)),
             f"{statistics.median(ms):,.0f}",
             f"{min(ms):,.0f}-{max(ms):,.0f}"]
            for (lab, (scene, spp, t)), ms in groups.items()]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", default="docs/data/nw10-speed.csv")
    args = parser.parse_args()
    rows = load(args.csv)
    last = rows[-1]["line"]
    covered = sum(end - start + 1 for start, end, _ in SESSIONS)
    if SESSIONS[-1][1] != last or covered != len(rows):
        sys.exit(f"SESSIONS covers {covered} rows ending at line "
                 f"{SESSIONS[-1][1]}; the CSV has {len(rows)} ending at line "
                 f"{last}. Add the new session.")
    for start, end, title in SESSIONS:
        session = [r for r in rows if start <= r["line"] <= end]
        print(f"## Lines {start}-{end}: {title}\n")
        table(["label", "scene", "settings", "n", "median ms", "min-max"],
              medians(session))
        table(["scene", "settings", "A", "B", "rounds", "A median ms",
               "B median ms", "speedup", "min-max", "B faster", "image"],
              paired(session))
        table(["label", "scene", "spp", "threads", "n", "median ms",
               "vs 1 thread"], threads(session))
        table(["scene", "spp", "from", "to", "rounds", "end to end",
               "min-max"], end_to_end(session))


if __name__ == "__main__":
    main()
