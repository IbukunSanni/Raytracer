#!/usr/bin/env bash
# Turn a numbered PNG sequence from the renderer into a video.
#
# The renderer already produces animations: a Lua script loops over keyframes and
# calls gr.render once per frame (see assets/scenes/final_animation.lua).
# This is the missing last step -- the frames never became a clip.
#
# Usage:
#   scripts/stitch_animation.sh <prefix> [fps] [output]
#
# Example, for renders/bkeytest_frame_001.png ... _097.png:
#   scripts/stitch_animation.sh renders/bkeytest_frame_ 24 ball.mp4
set -euo pipefail

PREFIX="${1:?usage: stitch_animation.sh <prefix> [fps] [output]}"
FPS="${2:-24}"
OUT="${3:-animation.mp4}"

# The renderer writes %03d (frame_001). Detect the width so %04d sequences work too.
FIRST=$(ls "${PREFIX}"*.png 2>/dev/null | head -1 || true)
if [ -z "$FIRST" ]; then
	echo "No frames matching ${PREFIX}*.png" >&2
	exit 1
fi
DIGITS=$(basename "$FIRST" .png | sed "s|^$(basename "$PREFIX")||" | wc -c)
DIGITS=$((DIGITS - 1))

START=$(ls "${PREFIX}"*.png | head -1 | sed "s|.*${PREFIX}||; s|\.png||" | sed 's/^0*//')
START=${START:-0}

echo "frames  : $(ls "${PREFIX}"*.png | wc -l)"
echo "pattern : ${PREFIX}%0${DIGITS}d.png  (starting at ${START})"
echo "fps     : ${FPS}"
echo "output  : ${OUT}"

# -start_number so a sequence that does not begin at 1 still works.
# yuv420p + the scale filter keep the result playable in browsers and
# QuickTime, which reject odd pixel dimensions.
ffmpeg -y \
	-framerate "$FPS" \
	-start_number "$START" \
	-i "${PREFIX}%0${DIGITS}d.png" \
	-c:v libx264 \
	-pix_fmt yuv420p \
	-vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" \
	-crf 18 \
	"$OUT"

echo "wrote ${OUT}"
