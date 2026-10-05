#!/usr/bin/env bash
#
# Benchmarks the mesh intersection paths and appends one CSV row per timed
# run to docs/data/step8-bvh.csv.
#
#   scripts/bench_bvh.sh [-r ROUNDS] [-m "linear recursive iterative"] SCENE...
#
# SCENE is a file name under assets/scenes without .lua, and must set a
# literal `output = '...'` so its image can be hashed. Build first; this
# script runs build/raytracer as it is.
#
# Three rules, each there because breaking it once produced a wrong number:
#
#   Interleaved, not blocked. Every round runs every scene in every mode,
#   so drift in the machine lands on all of them equally.
#
#   Hashes checked every round. Every mode must write a byte-identical
#   image for a scene. One mismatch aborts before anything is written.
#
#   Times and counts from separate runs. Timed runs leave RT_STATS off,
#   since counting slows the render it counts. One extra run per scene and
#   mode with RT_STATS=1 supplies the counts, which are deterministic, and
#   they are copied onto that pair's timed rows.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

usage() {
  echo "usage: $0 [-r ROUNDS] [-m \"MODES\"] SCENE..." >&2
  exit 1
}

ROUNDS=3
MODES="linear recursive iterative"
while getopts "r:m:" opt; do
  case $opt in
    r) ROUNDS=$OPTARG ;;
    m) MODES=$OPTARG ;;
    *) usage ;;
  esac
done
shift $((OPTIND - 1))
[ $# -gt 0 ] || usage
SCENES=("$@")

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

# Renders SCENE in MODE with RT_STATS=STATS; sets LINE (the bench record)
# and HASH (the image's md5).
render() {
  local scene=$1 mode=$2 stats=$3 log
  log=$(RT_STATS=$stats RT_LOG=off,render:debug BVH_TRAVERSAL=$mode \
    "$BIN" "assets/scenes/$scene.lua" 2>&1)
  LINE=$(grep -o "bench .*" <<<"$log")
  if [ -z "$LINE" ]; then
    echo "FAIL  no bench record from $scene ($mode):" >&2
    tail -5 <<<"$log" >&2
    exit 1
  fi
  HASH=$(md5sum "$OUTPUT" | cut -d' ' -f1)
}

# Every mode must match the first image seen for the scene.
declare -A REF_HASH
check_hash() {
  local scene=$1 mode=$2
  if [ -z "${REF_HASH[$scene]:-}" ]; then
    REF_HASH[$scene]=$HASH
  elif [ "${REF_HASH[$scene]}" != "$HASH" ]; then
    echo "FAIL  $scene ($mode) wrote a different image:" \
      "$HASH vs ${REF_HASH[$scene]}. Nothing written." >&2
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
    for mode in $MODES; do
      render "$scene" "$mode" ""
      check_hash "$scene" "$mode"
      echo "$scene $mode $round $(field render_ms "$LINE") $LINE" >>"$TIMED"
      echo "round $round/$ROUNDS  $scene  $mode  $(field render_ms "$LINE") ms"
    done
  done
done

declare -A COUNTS
for scene in "${SCENES[@]}"; do
  OUTPUT=$(output_of "$scene")
  for mode in $MODES; do
    render "$scene" "$mode" 1
    check_hash "$scene" "$mode"
    p=$(field rays_primary "$LINE")
    s=$(field rays_shadow "$LINE")
    b=$(field rays_bounce "$LINE")
    COUNTS[$scene/$mode]="$(field build_ms "$LINE"),$p,$s,$b,$((p + s + b)),\
$(field calls "$LINE"),$(field nodes "$LINE"),$(field triangles "$LINE")"
  done
done

[ -f "$CSV" ] || {
  mkdir -p "$(dirname "$CSV")"
  echo "date,commit,scene,width,height,spp,threads,traversal,split,leaf,\
round,render_ms,build_ms,rays_primary,rays_shadow,rays_bounce,rays_total,\
calls,nodes,triangles,image_md5" >"$CSV"
}

while read -r scene mode round ms line; do
  echo "$DATE,$COMMIT,$scene,$(field width " $line"),$(field height " $line"),\
$(field spp " $line"),$(field threads " $line"),$mode,\
$(field split " $line"),$(field leaf " $line"),$round,$ms,\
${COUNTS[$scene/$mode]},${REF_HASH[$scene]}" >>"$CSV"
done <"$TIMED"

echo
echo "appended $((ROUNDS * ${#SCENES[@]} * $(wc -w <<<"$MODES"))) rows to $CSV"
echo
printf "%-12s %-10s %5s %10s %8s %10s %10s %14s\n" \
  scene mode n mean_ms sd_ms min_ms max_ms rays_per_sec
for scene in "${SCENES[@]}"; do
  for mode in $MODES; do
    rays=$(cut -d, -f5 <<<"${COUNTS[$scene/$mode]}")
    awk -v s="$scene" -v m="$mode" -v rays="$rays" '
      $1 == s && $2 == m {
        t += $4; q += $4 * $4; n++
        if (min == "" || $4 < min) min = $4
        if ($4 > max) max = $4
      }
      END {
        mean = t / n; sd = sqrt(q / n - mean * mean)
        if (sd != sd) sd = 0
        printf "%-12s %-10s %5d %10.2f %8.2f %10.2f %10.2f %14.0f\n",
          s, m, n, mean, sd, min, max, rays / (mean / 1000)
      }' "$TIMED"
  done
done
