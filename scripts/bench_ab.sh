#!/usr/bin/env bash
#
# Paired A/B benchmark: times two binaries on the same scenes, alternating
# them inside every round, and reports the speed-up as a ratio.
#
#   scripts/bench_ab.sh [-r ROUNDS] [-p SPP] [-d] \
#       BIN_A LABEL_A BIN_B LABEL_B SCENE...
#
#   scripts/bench_ab.sh build/raytracer-before.exe cbb25e9:before \
#       build/raytracer 6048393-dirty:rung2 nw_final cornell_smoke
#
# Why paired: this laptop throttles. The same binary measured 29.3 s on
# 29 Oct and 35.5 then 41.0 s on 1 Nov, so an absolute time from one
# session can't be compared with one from another. Inside one round, A and
# B run seconds apart under the same heat, so the ratio A/B survives the
# drift even when both times move. The reported speed-up is the geometric
# mean of the per-round ratios (the right mean for ratios: 2x then 0.5x
# averages to 1x, not 1.25x).
#
# Order is ABBA: odd rounds run A first, even rounds B first, so a machine
# that heats up across a round doesn't always penalise the same binary.
#
#   -r ROUNDS  paired rounds (default 5).
#   -p SPP     sets RT_SPP, overriding the scene's samples per pixel.
#   -d         allow the two images to differ. Without it, A and B must
#              write byte-identical images, and a mismatch aborts before
#              anything is written: that is the gate for a rung that
#              should change nothing but speed.
#
# Rows go to BENCH_CSV (default docs/data/nw10-speed.csv) in bench_bvh.sh's
# columns, with the label in the commit column. The count columns are left
# empty: this script times, it doesn't count.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

usage() {
  echo "usage: $0 [-r ROUNDS] [-p SPP] [-d] BIN_A LABEL_A BIN_B LABEL_B" \
    "SCENE..." >&2
  exit 1
}

ROUNDS=5
SPP=""
ALLOW_DIFF=0
while getopts "r:p:d" opt; do
  case $opt in
    r) ROUNDS=$OPTARG ;;
    p) SPP=$OPTARG ;;
    d) ALLOW_DIFF=1 ;;
    *) usage ;;
  esac
done
shift $((OPTIND - 1))
[ $# -ge 5 ] || usage
BINS=("$1" "$3")
LABELS=("$2" "$4")
shift 4
SCENES=("$@")

CSV=${BENCH_CSV:-docs/data/nw10-speed.csv}
DATE=$(date +%F)

for bin in "${BINS[@]}"; do
  [ -x "$bin" ] || [ -x "$bin.exe" ] || {
    echo "FAIL  no $bin" >&2
    exit 1
  }
done

field() { sed -n "s/.* $1=\([^ ]*\).*/\1/p" <<<"$2"; }

output_of() {
  grep -o "output = '[^']*'" "assets/scenes/$1.lua" | head -1 |
    sed "s/output = '//;s/'$//"
}

for scene in "${SCENES[@]}"; do
  [ -n "$(output_of "$scene")" ] || {
    echo "FAIL  no assets/scenes/$scene.lua, or no literal output path" >&2
    exit 1
  }
done

# Renders SCENE with binary number WHICH (0 = A, 1 = B); sets LINE and HASH.
render() {
  local scene=$1 which=$2 log
  log=$(RT_SPP=$SPP RT_LOG=off,render:debug \
    "${BINS[$which]}" "assets/scenes/$scene.lua" 2>&1)
  LINE=$(grep -o "bench .*" <<<"$log")
  if [ -z "$LINE" ]; then
    echo "FAIL  no bench record from $scene (${LABELS[$which]}):" >&2
    tail -5 <<<"$log" >&2
    exit 1
  fi
  HASH=$(md5sum "$(output_of "$scene")" | cut -d' ' -f1)
}

START=$SECONDS
ROWS=$(mktemp)
PAIRS=$(mktemp)
trap 'rm -f "$ROWS" "$PAIRS"' EXIT

for round in $(seq 1 "$ROUNDS"); do
  for scene in "${SCENES[@]}"; do
    order=(0 1)
    [ $((round % 2)) = 0 ] && order=(1 0)
    declare -A MS=() MD5=()
    for which in "${order[@]}"; do
      render "$scene" "$which"
      MS[$which]=$(field render_ms "$LINE")
      MD5[$which]=$HASH
      echo "$DATE,${LABELS[$which]},$scene,$(field width "$LINE"),\
$(field height "$LINE"),$(field spp "$LINE"),$(field threads "$LINE"),\
iterative,$(field split "$LINE"),$(field leaf "$LINE"),$round,\
${MS[$which]},$(field build_ms "$LINE"),,,,,,,,$HASH" >>"$ROWS"
    done
    if [ "$ALLOW_DIFF" = 0 ] && [ "${MD5[0]}" != "${MD5[1]}" ]; then
      echo "FAIL  $scene: ${LABELS[1]} wrote a different image" \
        "(${MD5[1]} vs ${MD5[0]}). Nothing written. Pass -d if the" \
        "change is meant to alter the image." >&2
      exit 1
    fi
    echo "$scene ${MS[0]} ${MS[1]}" >>"$PAIRS"
    printf "round %d/%d  %-14s  A %8s ms  B %8s ms  %6.2fx\n" "$round" \
      "$ROUNDS" "$scene" "${MS[0]}" "${MS[1]}" \
      "$(awk -v a="${MS[0]}" -v b="${MS[1]}" 'BEGIN { print a / b }')"
  done
done

[ -f "$CSV" ] || {
  mkdir -p "$(dirname "$CSV")"
  echo "date,commit,scene,width,height,spp,threads,traversal,split,leaf,\
round,render_ms,build_ms,rays_primary,rays_shadow,rays_bounce,rays_total,\
calls,nodes,triangles,image_md5" >"$CSV"
}
if ! cat "$ROWS" >>"$CSV"; then
  KEPT=$(mktemp "${TMPDIR:-/tmp}/bench-rows-XXXXXX.csv")
  cp "$ROWS" "$KEPT"
  echo "FAIL  could not append to $CSV; the rows are in $KEPT" >&2
  exit 1
fi

echo
echo "appended $(wc -l <"$ROWS") rows to $CSV in $((SECONDS - START)) s"
echo "A = ${LABELS[0]}, B = ${LABELS[1]}"
echo
# speedup is A/B: above 1 means B is faster. min/max are the per-round
# ratios, so their spread shows how much the pairing still leaves.
awk '
  {
    if (!($1 in n)) order[++keys] = $1
    n[$1]++; r = $2 / $3; lg[$1] += log(r)
    a[$1, n[$1]] = $2; b[$1, n[$1]] = $3
    if (!($1 in lo) || r < lo[$1]) lo[$1] = r
    if (r > hi[$1]) hi[$1] = r
  }
  function median(s, arr,   i, v, c) {
    c = n[s]; for (i = 1; i <= c; i++) v[i] = arr[s, i]
    asort(v)
    return c % 2 ? v[(c + 1) / 2] : (v[c / 2] + v[c / 2 + 1]) / 2
  }
  END {
    printf "%-14s %3s %12s %12s %9s %9s %9s\n", "scene", "n", "A_median_ms",
      "B_median_ms", "speedup", "min", "max"
    for (i = 1; i <= keys; i++) {
      s = order[i]
      printf "%-14s %3d %12.0f %12.0f %8.2fx %8.2fx %8.2fx\n", s, n[s],
        median(s, a), median(s, b), exp(lg[s] / n[s]), lo[s], hi[s]
    }
  }' "$PAIRS"
