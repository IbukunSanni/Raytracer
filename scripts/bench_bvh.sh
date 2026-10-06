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
# script runs build/raytracer as it is. Every split is run with every mode;
# -s defaults to median alone.
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
#   since counting slows the render it counts. One extra run per scene and
#   configuration with RT_STATS=1 supplies the counts, which are
#   deterministic, and they are copied onto that configuration's timed rows.
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
CONFIGS=()
for split in $SPLITS; do
  for mode in $MODES; do CONFIGS+=("$split:$mode"); done
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
  local scene=$1 config=$2 stats=$3 log
  log=$(RT_STATS=$stats RT_LOG=off,render:debug BVH_SPLIT=${config%%:*} \
    BVH_TRAVERSAL=${config##*:} "$BIN" "assets/scenes/$scene.lua" 2>&1)
  LINE=$(grep -o "bench .*" <<<"$log")
  if [ -z "$LINE" ]; then
    echo "FAIL  no bench record from $scene ($config):" >&2
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

declare -A COUNTS
for scene in "${SCENES[@]}"; do
  OUTPUT=$(output_of "$scene")
  for config in "${CONFIGS[@]}"; do
    render "$scene" "$config" 1
    check_hash "$scene" "$config"
    p=$(field rays_primary "$LINE")
    s=$(field rays_shadow "$LINE")
    b=$(field rays_bounce "$LINE")
    COUNTS[$scene/$config]="$(field build_ms "$LINE"),$p,$s,$b,$((p + s + b)),\
$(field calls "$LINE"),$(field nodes "$LINE"),$(field triangles "$LINE")"
  done
done

[ -f "$CSV" ] || {
  mkdir -p "$(dirname "$CSV")"
  echo "date,commit,scene,width,height,spp,threads,traversal,split,leaf,\
round,render_ms,build_ms,rays_primary,rays_shadow,rays_bounce,rays_total,\
calls,nodes,triangles,image_md5" >"$CSV"
}

while read -r scene config round ms line; do
  echo "$DATE,$COMMIT,$scene,$(field width " $line"),$(field height " $line"),\
$(field spp " $line"),$(field threads " $line"),${config##*:},\
$(field split " $line"),$(field leaf " $line"),$round,$ms,\
${COUNTS[$scene/$config]},${REF_HASH[$scene/${config%%:*}]}" >>"$CSV"
done <"$TIMED"

echo
echo "appended $((ROUNDS * ${#SCENES[@]} * ${#CONFIGS[@]})) rows to $CSV"
echo
printf "%-12s %-18s %5s %10s %8s %10s %10s %14s\n" \
  scene config n mean_ms sd_ms min_ms max_ms rays_per_sec
for scene in "${SCENES[@]}"; do
  for config in "${CONFIGS[@]}"; do
    rays=$(cut -d, -f5 <<<"${COUNTS[$scene/$config]}")
    awk -v s="$scene" -v c="$config" -v rays="$rays" '
      $1 == s && $2 == c {
        t += $4; q += $4 * $4; n++
        if (min == "" || $4 < min) min = $4
        if ($4 > max) max = $4
      }
      END {
        mean = t / n; sd = sqrt(q / n - mean * mean)
        if (sd != sd) sd = 0
        printf "%-12s %-18s %5d %10.2f %8.2f %10.2f %10.2f %14.0f\n",
          s, c, n, mean, sd, min, max, rays / (mean / 1000)
      }' "$TIMED"
  done
done
