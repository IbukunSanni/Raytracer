#!/usr/bin/env bash
# Hashes the renders of a fixed scene set, to prove a change altered nothing.
#
#   scripts/baseline.sh              # write docs/data/baseline.sha256
#   scripts/baseline.sh --check      # render again and compare against it
#
# Run from the repo root with a Release build in build/. Output is
# deterministic for a fixed thread count and sample count, so both are pinned
# here; compare only builds from the same compiler (libm differences move a
# fraction of a percent of pixels between GCC and MSVC).
#
# Scenes with image textures render the placeholder checker on a machine
# without the textures, so a baseline is only comparable on the machine, and
# with the textures, it was made with.
set -euo pipefail

SCENES="simple hier instance metal_spheres mirror_spheres nonhier2 template
        glass_spheres glass_spheres_camera caustic thin_lens checkered_floor
        uv_checker mesh_uv macho-cows"
OUT="docs/data/baseline.sha256"
export RT_LOG=off RT_THREADS=8 RT_SPP=4

marker=$(mktemp)
for scene in $SCENES; do
  ./build/raytracer "assets/scenes/$scene.lua" > /dev/null
done

hashes=$(find renders -name '*.png' -newer "$marker" -print0 | sort -z \
         | xargs -0 sha256sum | sed -E 's@ [ *]renders/@  @')
rm -f "$marker"

if [ "${1:-}" = "--check" ]; then
  if diff <(echo "$hashes") <(grep -v '^#' "$OUT"); then
    echo "identical: $(echo "$hashes" | wc -l) renders"
  else
    echo "CHANGED (lines above: < now, > baseline)"; exit 1
  fi
else
  { echo "# $(git rev-parse --short HEAD), RT_THREADS=8 RT_SPP=4, $(date +%F)"
    echo "$hashes"; } > "$OUT"
  echo "wrote $OUT: $(echo "$hashes" | wc -l) renders"
fi
