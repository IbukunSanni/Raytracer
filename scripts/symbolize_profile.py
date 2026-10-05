"""Maps scripts/sample_profile.cc output to functions through `nm`.

    python scripts/symbolize_profile.py PROFILE EXE [TOP]

Prints each function's share of render-worker samples, most first. Samples
in system DLLs are reported as one line; main-thread samples are counted
but left out, since the main thread spends a render waiting in join.
"""
import bisect, collections, subprocess, sys

samples_path, exe = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
image_base = 0x140000000

syms = []
for line in subprocess.run(["nm", "-C", "--defined-only", exe],
                           capture_output=True, text=True).stdout.splitlines():
    parts = line.split(" ", 2)
    if len(parts) == 3 and parts[1] in "Tt":
        syms.append((int(parts[0], 16) - image_base, parts[2]))
syms.sort()
addrs = [a for a, _ in syms]

def name(rva):
    i = bisect.bisect_right(addrs, rva) - 1
    if i < 0:
        return "?"
    n = syms[i][1]
    return n.split("(")[0] if "(" in n else n

counts = collections.Counter()
ext = {"w": 0, "m": 0}
total = {"w": 0, "m": 0}
for line in open(samples_path):
    tag, val = line.split()
    if tag.endswith("ext"):
        ext[tag[0]] += int(val)
        continue
    total[tag] += 1
    if tag == "w":
        counts[name(int(val, 16))] += 1

w = total["w"] + ext["w"]
print(f"worker samples: {w} ({total['w']} in exe, {ext['w']} in system DLLs "
      f"= {100 * ext['w'] / max(w, 1):.1f}%)")
print(f"main-thread samples: {total['m'] + ext['m']} (not counted below)")
for fn, c in counts.most_common(top):
    print(f"{100 * c / w:6.2f}%  {c:7d}  {fn}")
