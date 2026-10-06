#!/usr/bin/env bash
#
# Benchmarks the mesh intersection paths and appends one CSV row per timed
# run to docs/data/step8-bvh.csv.
#
#   scripts/bench_bvh.sh [-r ROUNDS] [-m "linear recursive iterative"]
#                        [-s "median sah"] SCENE...
#
# SCENE is a file name under assets/scenes without .lua, and must set a
# literal `output = '...'` so its image can be hashed. Build first; this
# script runs build/raytracer as it is. Every split is run with every tree
# mode; -s defaults to median alone. `linear` never reads the tree, so it
# runs under the first split only.
#
# scripts/bvh_summary.py turns the CSV into the tables the docs quote.
#
# Three rules, each there because breaking it once produced a wrong number:
#
#   Interleaved, not blocked, and rotated. Every round runs every scene in
#   every configuration, so drift in the machine lands on all of them
#   equally. Each round also starts one configuration later: with a fixed
#   order, whichever ran second measured ~6% slow on a 30 ms scene.
#
#   Hashes checked every round. Within one split, every mode must write a
#   byte-identical image for a scene; one mismatch aborts before anything is
#   written. Across splits images may differ: two triangles at exactly the
#   same distance are a tie, and a different tree can break it differently.
#
#   Times and counts from separate runs. Timed runs leave RT_STATS off,
#   since counting slows the render it counts. After the timed rounds come
#   as many counting rounds with RT_STATS=1, in the same rotated order. The
#   counts are deterministic, so they must agree in every round or nothing
#   is written; build_ms is a time, so each row takes its own round's.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

usage() {
  echo "usage: $0 [-r ROUNDS] [-m \"MODES\"] [-s \"SPLITS\"] SCENE..." >&2
  exit 1
}

ROUNDS=3
MODES="linear recursive iterative"
SPLITS="median"
while getopts "r:m:s:" opt; do
  case $opt in
    r) ROUNDS=$OPTARG ;;
    m) MODES=$OPTARG ;;
    s) SPLITS=$OPTARG ;;
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
  log=$(RT_STATS=$stats RT_LOG=off,render:debug BVH_SPLIT=${config%%:*} \
    BVH_TRAVERSAL=${config##*:} "$BIN" "assets/scenes/$scene.lua" 2>&1)
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

declare -A COUNTS BUILD_MS
for round in $(seq 1 "$ROUNDS"); do
  for scene in "${SCENES[@]}"; do
    OUTPUT=$(output_of "$scene")
    for k in "${!CONFIGS[@]}"; do
      config=${CONFIGS[$(((k + round - 1) % ${#CONFIGS[@]}))]}
      render "$scene" "$config" 1
      check_hash "$scene" "$config"
      p=$(field rays_primary "$LINE")
      s=$(field rays_shadow "$LINE")
      b=$(field rays_bounce "$LINE")
      counts="$p,$s,$b,$((p + s + b)),$(field calls "$LINE"),\
$(field nodes "$LINE"),$(field triangles "$LINE")"
      key=$scene/$config
      if [ -z "${COUNTS[$key]:-}" ]; then
        COUNTS[$key]=$counts
      elif [ "${COUNTS[$key]}" != "$counts" ]; then
        echo "FAIL  $scene ($config) counted $counts in round $round," \
          "${COUNTS[$key]} before. Nothing written." >&2
        exit 1
      fi
      BUILD_MS[$key/$round]=$(field build_ms "$LINE")
      echo "counts $round/$ROUNDS  $scene  $config  build $(field build_ms "$LINE") ms"
    done
  done
done

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
${BUILD_MS[$scene/$config/$round]},${COUNTS[$scene/$config]},\
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
echo "appended $(wc -l <"$ROWS") rows to $CSV"
echo
# sd is the sample standard deviation (n - 1); rays/sec is the configuration's
# ray total over its mean render time.
awk -F, '
  {
    key = $3 " " $9 ":" $8
    if (!(key in n)) order[++keys] = key
    n[key]++; t[key] += $12; q[key] += $12 * $12
    bt[key] += $13; bq[key] += $13 * $13; rays[key] = $17
    if (!(key in lo) || $12 < lo[key]) lo[key] = $12
    if ($12 > hi[key]) hi[key] = $12
  }
  function sd(sum, sq, k) {
    return k > 1 ? sqrt(((sq - sum * sum / k) > 0 ? sq - sum * sum / k : 0) / (k - 1)) : 0
  }
  END {
    printf "%-12s %-18s %4s %10s %8s %10s %10s %9s %7s %14s\n", "scene", \
      "config", "n", "mean_ms", "sd_ms", "min_ms", "max_ms", "build_ms", "sd", \
      "rays_per_sec"
    for (i = 1; i <= keys; i++) {
      k = order[i]; split(k, part, " "); m = t[k] / n[k]
      printf "%-12s %-18s %4d %10.2f %8.2f %10.2f %10.2f %9.3f %7.3f %14.0f\n",
        part[1], part[2], n[k], m, sd(t[k], q[k], n[k]), lo[k], hi[k],
        bt[k] / n[k], sd(bt[k], bq[k], n[k]), rays[k] / (m / 1000)
    }
  }' "$ROWS"
