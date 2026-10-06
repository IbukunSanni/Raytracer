"""Turns the BVH bench CSV into the tables the docs quote.

    python scripts/bvh_summary.py [--csv PATH] [--commit C ...]
                                  [--before C --after C [--control SPLIT]]

Every BVH figure in the docs is computed here from rows that
scripts/bench_bvh.sh wrote, so each table can be regenerated and checked.
Output is Markdown.

  mean, sd      over one configuration's timed rounds. sd is the sample
                standard deviation (divides by n - 1).
  Mrays/s       rays_total / mean render time / 10^6. rays_total is
                primary + shadow + bounce, from the counting runs.
  per ray       calls (mesh queries), nodes visited and triangles tested,
                each divided by rays_total.
  build_ms      mean and sd over the counting runs, one per round. Rows
                from before build times were taken every round carry one
                run's figure on every row, so their sd prints as 0.
  paired        two configurations of one scene in one bench run, matched
                by round, since a round runs every configuration back to
                back. B/A is the geometric mean of the per-round ratios
                B ms / A ms, and speedup is its inverse; "B faster" counts
                the rounds where B/A < 1.
  before/after  one configuration in two commits. Those runs are not
                interleaved, so drift between them (up to ~7% between days
                on this machine) is not cancelled; build times are compared
                the same way.
  / control     with --control SPLIT, for a change that leaves that split's
                code alone: a configuration's after/before divided by the
                control's after/before in the same scene and traversal. The
                control moved only with the machine, so the quotient is the
                change with the drift between the two runs taken out.

Counts and image hashes must be identical across a configuration's rows;
the script stops if they are not.
"""

import argparse
import collections
import csv
import math
import statistics
import sys

PAIRS = [
    (("median", "recursive"), ("sah", "recursive")),
    (("median", "iterative"), ("sah", "iterative")),
    (("median", "recursive"), ("median", "iterative")),
    (("sah", "recursive"), ("sah", "iterative")),
    (("median", "linear"), ("median", "iterative")),
    (("median", "linear"), ("sah", "iterative")),
]


def load(path, commits):
    with open(path, newline="") as f:
        rows = [r for r in csv.DictReader(f)
                if not commits or r["commit"] in commits]
    seen = set()
    for r in rows:
        key = (r["date"], r["commit"], r["scene"], r["split"], r["traversal"],
               r["round"])
        if key in seen:
            sys.exit(f"round {key} appears twice: two bench runs of one scene "
                     "on one day and commit cannot be paired")
        seen.add(key)
    return rows


def sd(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


def summarize(group):
    first = group[0]
    for name in ("rays_total", "calls", "nodes", "triangles", "image_md5"):
        if any(r[name] != first[name] for r in group):
            sys.exit(f"{name} differs between rounds of {first['commit']} "
                     f"{first['scene']} {first['split']}:{first['traversal']}")
    ms = [float(r["render_ms"]) for r in group]
    build = [float(r["build_ms"]) for r in group]
    rays = int(first["rays_total"])
    mean = statistics.fmean(ms)
    return {
        "n": len(ms), "mean": mean, "sd": sd(ms), "min": min(ms),
        "max": max(ms), "mrays": rays / (mean / 1000) / 1e6,
        "build": statistics.fmean(build), "build_sd": sd(build),
        "calls": int(first["calls"]) / rays,
        "nodes": int(first["nodes"]) / rays,
        "tris": int(first["triangles"]) / rays,
        "md5": first["image_md5"][:8],
        "by_round": {int(r["round"]): float(r["render_ms"]) for r in group},
        "counts": tuple(first[k] for k in ("rays_total", "calls", "nodes",
                                           "triangles")),
    }


def table(header, lines):
    print("| " + " | ".join(header) + " |")
    print("|" + "---|" * len(header))
    for line in lines:
        print("| " + " | ".join(line) + " |")
    print()


def configurations(rows):
    groups = collections.defaultdict(list)
    for r in rows:
        groups[(r["date"], r["commit"], r["scene"], r["split"],
                r["traversal"])].append(r)
    return {key: summarize(sorted(g, key=lambda r: int(r["round"])))
            for key, g in groups.items()}


def print_configurations(configs):
    print("## Per configuration\n")
    lines = []
    for (date, commit, scene, split, mode), s in configs.items():
        lines.append([
            date, commit, scene, f"{split}:{mode}", str(s["n"]),
            f"{s['mean']:.2f}", f"{s['sd']:.2f}",
            f"{s['min']:.2f}-{s['max']:.2f}", f"{s['mrays']:.3g}",
            f"{s['build']:.3f} ({s['build_sd']:.3f})", f"{s['calls']:.2f}",
            f"{s['nodes']:.2f}", f"{s['tris']:.3f}", s["md5"]])
    table(["date", "commit", "scene", "config", "n", "mean ms", "sd",
           "min-max", "Mrays/s", "build ms (sd)", "queries/ray", "nodes/ray",
           "tris/ray", "image"], lines)


def print_pairs(configs):
    print("## Paired, within one bench run\n")
    runs = collections.defaultdict(dict)
    for (date, commit, scene, split, mode), s in configs.items():
        runs[(date, commit, scene)][(split, mode)] = s
    lines = []
    for (date, commit, scene), by_config in runs.items():
        for a_key, b_key in PAIRS:
            if a_key not in by_config or b_key not in by_config:
                continue
            a, b = by_config[a_key], by_config[b_key]
            rounds = sorted(set(a["by_round"]) & set(b["by_round"]))
            ratios = [b["by_round"][k] / a["by_round"][k] for k in rounds]
            geo = math.exp(statistics.fmean(math.log(x) for x in ratios))
            faster = sum(x < 1 for x in ratios)
            lines.append([
                commit, scene, ":".join(a_key), ":".join(b_key),
                str(len(rounds)), f"{a['mean']:.2f}", f"{b['mean']:.2f}",
                f"{geo:.3f}", f"{1 / geo:.2f}x", f"{faster}/{len(rounds)}",
                f"{a['tris']:.3f} -> {b['tris']:.3f} "
                f"({100 * (b['tris'] / a['tris'] - 1):+.1f}%)",
                f"{a['nodes']:.2f} -> {b['nodes']:.2f}"])
    table(["commit", "scene", "A", "B", "rounds", "A ms", "B ms", "B/A",
           "speedup", "B faster", "tris/ray A -> B", "nodes/ray A -> B"], lines)


def print_before_after(configs, before, after, control):
    print(f"## {before} -> {after}\n")
    by_commit = collections.defaultdict(dict)
    for (date, commit, scene, split, mode), s in configs.items():
        key = (scene, split, mode)
        if key in by_commit[commit]:
            sys.exit(f"{commit} {key} was benched on two dates; pass the "
                     "rows of one run")
        by_commit[commit][key] = s
    def ratios(key):
        b, a = by_commit[before].get(key), by_commit[after].get(key)
        if b is None or a is None:
            return None
        return a["mean"] / b["mean"], a["build"] / b["build"]

    lines = []
    for key, b in by_commit[before].items():
        if key not in by_commit[after]:
            continue
        a = by_commit[after][key]
        ms_ratio, build_ratio = ratios(key)
        ctrl = ratios((key[0], control, key[2])) if control else None
        if ctrl is None or key[1] == control:
            corrected = ["-", "-"]
        else:
            corrected = [f"{ms_ratio / ctrl[0]:.3f}",
                         f"{build_ratio / ctrl[1]:.3f}"]
        lines.append([
            key[0], f"{key[1]}:{key[2]}",
            f"{b['mean']:.2f} ({b['sd']:.2f})", f"{a['mean']:.2f} ({a['sd']:.2f})",
            f"{ms_ratio:.3f}", corrected[0],
            f"{b['build']:.3f} ({b['build_sd']:.3f})",
            f"{a['build']:.3f} ({a['build_sd']:.3f})",
            f"{build_ratio:.3f}", corrected[1],
            "same" if a["counts"] == b["counts"] else "differ",
            "same" if a["md5"] == b["md5"] else "differ"])
    table(["scene", "config", "before ms (sd)", "after ms (sd)", "after/before",
           "/ control", "build before (sd)", "build after (sd)",
           "after/before", "/ control", "counts", "image"], lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", default="docs/data/step8-bvh.csv")
    parser.add_argument("--commit", nargs="*", default=[])
    parser.add_argument("--before")
    parser.add_argument("--after")
    parser.add_argument("--control")
    args = parser.parse_args()
    commits = set(args.commit)
    if args.before and args.after and commits:
        commits |= {args.before, args.after}
    configs = configurations(load(args.csv, commits))
    print_configurations(configs)
    print_pairs(configs)
    if args.before and args.after:
        print_before_after(configs, args.before, args.after, args.control)


if __name__ == "__main__":
    main()
