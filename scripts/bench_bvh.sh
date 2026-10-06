#!/usr/bin/env bash
#
# Benchmarks the mesh intersection paths and appends one CSV row per timed
# run to docs/data/step8-bvh.csv.
#
#   scripts/bench_bvh.sh [-r ROUNDS] [-m "linear recursive iterative"]
#                        [-s "median sah"] [-p SPP] SCENE...
#
# SCENE is a file name under assets/scenes without .lua, and must set a
# literal `output = '...'` so its image can be hashed. Build first; this
# script runs build/raytracer as it is.
#
# Defaults are the minimum that still answers an open question, because a
# full matrix took ~10 minutes on one scene to re-confirm settled answers:
#
#   -r 5         timed rounds. Rotation keeps them fair (below), and the
#                summary's median shrugs off the throttling outliers.
#   -m iterative only. Recursive against iterative is settled -- within
#                ~1-2% in three separate runs -- so recursive is checked,
#                not timed: one gate run per scene and split (below).
#                `linear` is ~190x slower and its numbers are recorded; ask
#                for it with -m when a before-number is needed.
#   -s median    every split is run with every mode. `linear` never reads
#                the tree, so it runs under the first split only.
#   -p SPP       sets RT_SPP, overriding the scene's samples per pixel. Time
#                per pixel-sample is linear in spp (2.009x for 2x), so a
#                slow scene can be compared at a few spp.
#
# scripts/bvh_summary.py turns the CSV into the tables the docs quote.
#
# Four rules, each there because breaking it once produced a wrong number:
#
#   Interleaved, not blocked, and rotated. Every round runs every scene in
#   every configuration, so drift in the machine lands on all of them
#   equally. Each round also starts one configuration later: with a fixed
#   order, whichever ran second measured ~6% slow on a 30 ms scene.
#
#   Hashes checked every run. Within one split, every mode must write a
#   byte-identical image for a scene; one mismatch aborts before anything is
#   written. Across splits images may differ: two triangles at exactly the
#   same distance are a tie, and a different tree can break it differently.
#
#   Times and counts from separate runs. Timed runs leave RT_STATS off,
#   since counting slows the render it counts; each still records its own
#   build_ms, which is always measured, so build time keeps a per-round
#   spread (that spread is what measured sort -> nth_element). Counts come
#   from two RT_STATS=1 runs per configuration afterwards: they are
#   deterministic, so one would do, and the second is the check that they
#   are. A destructor-based flush once gave wrong totals in 18 of 30 runs.
#
#   Recursive and iterative must agree to the digit. When recursive is not
#   being timed, one counting run of it per scene and split must match
#   iterative's image and counts exactly.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

usage() {
  echo "usage: $0 [-r ROUNDS] [-m \"MODES\"] [-s \"SPLITS\"] [-p SPP] SCENE..." >&2
  exit 1
}

ROUNDS=5
MODES="iterative"
SPLITS="median"
COUNT_ROUNDS=2  # one to get the counts, one to check they are deterministic
SPP=""
while getopts "r:m:s:p:" opt; do
  case $opt in
    r) ROUNDS=$OPTARG ;;
    m) MODES=$OPTARG ;;
    s) SPLITS=$OPTARG ;;
    p) SPP=$OPTARG ;;
    *) usage ;;
  esac
done
shift $((OPTIND - 1))
[ $# -gt 0 ] || usage
SCENES=("$@")

# A configuration is split:mode, one word, so it can key arrays and rows.
read -ra SPLIT_LIST <<<"$SPLITS"
CONFIGS=()
for split in "${SPLIT_LIST[@]}"; do
  for mode in $MODES; do
    [ "$mode" = linear ] && [ "$split" != "${SPLIT_LIST[0]}" ] && continue
    CONFIGS+=("$split:$mode")
  done
done

# The recursive gate runs only when iterative is timed and recursive is not.
GATE=0
[[ " $MODES " == *" iterative "* && " $MODES " != *" recursive "* ]] && GATE=1

BIN=build/raytracer
CSV=${BENCH_CSV:-docs/data/step8-bvh.csv}  # override for a dry run
DATE=$(date +%F)
# -dirty marks numbers from source that no commit contains.
COMMIT=$(git rev-parse --short HEAD)
git diff --quiet HEAD -- src || COMMIT="$COMMIT-dirty"

[ -x "$BIN" ] || [ -x "$BIN.exe" ] || {
  echo "no $BIN -- build first" >&2
  exit 1
}

# The value of KEY in a bench record.
field() { sed -n "s/.* $1=\([^ ]*\).*/\1/p" <<<"$2"; }

output_of() {
  grep -o "output = '[^']*'" "assets/scenes/$1.lua" | head -1 |
    sed "s/output = '//;s/'$//"
}

# Renders SCENE in CONFIG with RT_STATS=STATS; sets LINE (the bench record)
# and HASH (the image's md5).
render() {
  local scene=$1 config=$2 stats=$3 log status
  log=$(RT_STATS=$stats RT_SPP=$SPP RT_LOG=off,render:debug \
    BVH_SPLIT=${config%%:*} BVH_TRAVERSAL=${config##*:} \
    "$BIN" "assets/scenes/$scene.lua" 2>&1)
  status=$?
  LINE=$(grep -o "bench .*" <<<"$log")
  if [ -z "$LINE" ]; then
    echo "FAIL  no bench record from $scene ($config), exit status" \
      "$status:" >&2
    tail -5 <<<"$log" >&2
    exit 1
  fi
  HASH=$(md5sum "$OUTPUT" | cut -d' ' -f1)
}

# Every mode must match the first image seen for the scene and split.
declare -A REF_HASH
check_hash() {
  local key=$1/${2%%:*}
  if [ -z "${REF_HASH[$key]:-}" ]; then
    REF_HASH[$key]=$HASH
  elif [ "${REF_HASH[$key]}" != "$HASH" ]; then
    echo "FAIL  $1 ($2) wrote a different image:" \
      "$HASH vs ${REF_HASH[$key]}. Nothing written." >&2
    exit 1
  fi
}

# The counts in LINE, as the CSV columns they fill.
counts_of() {
  local p s b
  p=$(field rays_primary "$LINE")
  s=$(field rays_shadow "$LINE")
  b=$(field rays_bounce "$LINE")
  echo "$p,$s,$b,$((p + s + b)),$(field calls "$LINE"),\
$(field nodes "$LINE"),$(field triangles "$LINE")"
}

for scene in "${SCENES[@]}"; do
  [ -f "assets/scenes/$scene.lua" ] || {
    echo "FAIL  no assets/scenes/$scene.lua" >&2
    exit 1
  }
  [ -n "$(output_of "$scene")" ] || {
    echo "FAIL  $scene has no literal output path to hash" >&2
    exit 1
  }
done

START=$SECONDS
TIMED=$(mktemp)
trap 'rm -f "$TIMED"' EXIT

for round in $(seq 1 "$ROUNDS"); do
  for scene in "${SCENES[@]}"; do
    OUTPUT=$(output_of "$scene")
    for k in "${!CONFIGS[@]}"; do
      config=${CONFIGS[$(((k + round - 1) % ${#CONFIGS[@]}))]}
      render "$scene" "$config" ""
      check_hash "$scene" "$config"
      echo "$scene $config $round $(field render_ms "$LINE") $LINE" >>"$TIMED"
      echo "round $round/$ROUNDS  $scene  $config  $(field render_ms "$LINE") ms"
    done
  done
done

declare -A COUNTS
for round in $(seq 1 "$COUNT_ROUNDS"); do
  for scene in "${SCENES[@]}"; do
    OUTPUT=$(output_of "$scene")
    for k in "${!CONFIGS[@]}"; do
      config=${CONFIGS[$(((k + round - 1) % ${#CONFIGS[@]}))]}
      render "$scene" "$config" 1
      check_hash "$scene" "$config"
      counts=$(counts_of)
      key=$scene/$config
      if [ -z "${COUNTS[$key]:-}" ]; then
        COUNTS[$key]=$counts
      elif [ "${COUNTS[$key]}" != "$counts" ]; then
        echo "FAIL  $scene ($config) counted $counts in round $round," \
          "${COUNTS[$key]} before. Nothing written." >&2
        exit 1
      fi
      echo "counts $round/$COUNT_ROUNDS  $scene  $config"
    done
  done
done

if [ "$GATE" = 1 ]; then
  for scene in "${SCENES[@]}"; do
    OUTPUT=$(output_of "$scene")
    for split in "${SPLIT_LIST[@]}"; do
      render "$scene" "$split:recursive" 1
      check_hash "$scene" "$split:recursive"
      if [ "$(counts_of)" != "${COUNTS[$scene/$split:iterative]}" ]; then
        echo "FAIL  $scene ($split): recursive counted $(counts_of)," \
          "iterative ${COUNTS[$scene/$split:iterative]}. Nothing written." >&2
        exit 1
      fi
      echo "gate  $scene  $split  recursive matches iterative: image and counts"
    done
  done
fi

[ -f "$CSV" ] || {
  mkdir -p "$(dirname "$CSV")"
  echo "date,commit,scene,width,height,spp,threads,traversal,split,leaf,\
round,render_ms,build_ms,rays_primary,rays_shadow,rays_bounce,rays_total,\
calls,nodes,triangles,image_md5" >"$CSV"
}

ROWS=$(mktemp)
trap 'rm -f "$TIMED" "$ROWS"' EXIT
while read -r scene config round ms line; do
  echo "$DATE,$COMMIT,$scene,$(field width " $line"),$(field height " $line"),\
$(field spp " $line"),$(field threads " $line"),${config##*:},\
$(field split " $line"),$(field leaf " $line"),$round,$ms,\
$(field build_ms " $line"),${COUNTS[$scene/$config]},\
${REF_HASH[$scene/${config%%:*}]}" >>"$ROWS"
done <"$TIMED"
# A spreadsheet holding the CSV open makes the append fail; keep the rows.
if ! cat "$ROWS" >>"$CSV"; then
  KEPT=$(mktemp "${TMPDIR:-/tmp}/bench-rows-XXXXXX.csv")
  cp "$ROWS" "$KEPT"
  echo "FAIL  could not append to $CSV; the rows are in $KEPT" >&2
  exit 1
fi

echo
echo "appended $(wc -l <"$ROWS") rows to $CSV in $((SECONDS - START)) s"
echo
# sd is the sample standard deviation (n - 1); the median resists the
# outliers a throttling laptop produces; rays/sec is the configuration's ray
# total over its median render time.
awk -F, '
  {
    key = $3 " " $9 ":" $8
    if (!(key in n)) order[++keys] = key
    n[key]++; t[key] += $12; q[key] += $12 * $12
    v[key, n[key]] = $12; rays[key] = $17
    bt[key] += $13; bq[key] += $13 * $13
    if (!(key in lo) || $12 < lo[key]) lo[key] = $12
    if ($12 > hi[key]) hi[key] = $12
  }
  function sd(sum, sq, k) {
    return k > 1 ? sqrt(((sq - sum * sum / k) > 0 ? sq - sum * sum / k : 0) / (k - 1)) : 0
  }
  function median(k,   i, a, c) {
    c = n[k]; for (i = 1; i <= c; i++) a[i] = v[k, i]
    asort(a)
    return c % 2 ? a[(c + 1) / 2] : (a[c / 2] + a[c / 2 + 1]) / 2
  }
  END {
    printf "%-12s %-18s %4s %10s %10s %8s %10s %10s %9s %7s %14s\n", "scene", \
      "config", "n", "median_ms", "mean_ms", "sd_ms", "min_ms", "max_ms", \
      "build_ms", "sd", "rays_per_sec"
    for (i = 1; i <= keys; i++) {
      k = order[i]; split(k, part, " "); md = median(k)
      printf "%-12s %-18s %4d %10.2f %10.2f %8.2f %10.2f %10.2f %9.3f %7.3f %14.0f\n",
        part[1], part[2], n[k], md, t[k] / n[k], sd(t[k], q[k], n[k]),
        lo[k], hi[k], bt[k] / n[k], sd(bt[k], bq[k], n[k]), rays[k] / (md / 1000)
    }
  }' "$ROWS"
