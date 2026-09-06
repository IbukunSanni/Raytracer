#!/usr/bin/env bash
# Regression tests. Run from the repo root:
#
#     tests/run_tests.sh [path-to-raytracer]
#
# Defaults to ./build/raytracer. Each test either prints PASS or explains
# what differed; the script exits non-zero if anything failed.
set -uo pipefail

RT="${1:-./build/raytracer}"
[ -x "$RT" ] || RT="${RT}.exe"
if [ ! -x "$RT" ]; then
	echo "raytracer binary not found (looked for ${1:-./build/raytracer})" >&2
	exit 1
fi

mkdir -p tests/out
fails=0

pass() { echo "  PASS  $1"; }
fail() { echo "  FAIL  $1"; fails=$((fails + 1)); }

# --- 1. nested transforms -------------------------------------------------
# A child under a GeometryNode must render identically to the same child
# under a plain node. Guards against the transform being applied twice.
echo "nested transforms"
"$RT" tests/scenes/nested_control.lua          > /dev/null 2>&1
"$RT" tests/scenes/nested_under_geometry.lua   > /dev/null 2>&1
if [ -f tests/out/nested_control.png ] && [ -f tests/out/nested_under_geometry.png ]; then
	if cmp -s tests/out/nested_control.png tests/out/nested_under_geometry.png; then
		pass "child under GeometryNode matches child under plain node"
	else
		fail "GeometryNode child is displaced -- transform applied more than once"
	fi
else
	fail "renders were not produced"
fi

# --- 2. resolution independence -------------------------------------------
# The background used to be sampled by a 1:1 centre crop, which indexed out
# of bounds once the render exceeded the texture's size (920x891).
echo "resolution independence"
for res in 512 1024 2048; do
	cat > tests/out/_res.lua <<LUA
mat = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25)
scene = gr.node('scene')
s = gr.nh_sphere('s', {0, 0, -400}, 100)
s:set_material(mat)
scene:add_child(s)
l = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})
gr.render(scene, 'tests/out/res_${res}.png', ${res}, ${res},
          {0, 0, 800}, {0, 0, -800}, {0, 1, 0}, 50, {0.3, 0.3, 0.3}, {l})
LUA
	if "$RT" tests/out/_res.lua > /dev/null 2>&1 && [ -f "tests/out/res_${res}.png" ]; then
		pass "${res}x${res}"
	else
		fail "${res}x${res} did not render (exit $?)"
	fi
done
rm -f tests/out/_res.lua

# --- 3. sRGB transfer is applied and can be switched off -----------------
# srgb = false and srgb = true must produce different bytes; identical
# output means the transfer is stuck on or off.
echo "sRGB transfer toggle"
PROBE_SRGB=0 "$RT" tests/scenes/tonemap_probe.lua > /dev/null 2>&1
cp -f tests/out/tonemap_probe.png tests/out/tonemap_linear.png 2>/dev/null
PROBE_SRGB=1 "$RT" tests/scenes/tonemap_probe.lua > /dev/null 2>&1
cp -f tests/out/tonemap_probe.png tests/out/tonemap_srgb.png 2>/dev/null
if [ -s tests/out/tonemap_linear.png ] && [ -s tests/out/tonemap_srgb.png ]; then
	if cmp -s tests/out/tonemap_linear.png tests/out/tonemap_srgb.png; then
		fail "linear and sRGB dumps are identical -- transfer function not toggling"
	else
		pass "srgb = false and srgb = true produce different output"
	fi
else
	fail "tonemap probe did not render"
fi

# --- 4. BVH agrees with the linear scan -----------------------------------
# Only meaningful once BVH::build() is implemented; until then the renderer
# falls back to the linear scan and this trivially passes.
echo "BVH vs linear scan"
out=$(BVH_VERIFY=1 "$RT" assets/scenes/hier.lua 2>&1)
if echo "$out" | grep -q "BVH MISMATCH"; then
	fail "BVH disagrees with the linear scan"
	echo "$out" | grep "BVH MISMATCH" | head -3
else
	pass "no mismatches"
fi

# --- 5. BSDF furnace test -------------------------------------------------
# Separate binary: integrates the BSDFs directly, with no scene and no
# image, so a failure names a material rather than a render.
echo "BSDF furnace"
FURNACE="$(dirname "$RT")/furnace"
[ -x "$FURNACE" ] || FURNACE="${FURNACE}.exe"
if [ -x "$FURNACE" ]; then
	out=$("$FURNACE" 2>&1)
	if [ $? -eq 0 ]; then
		pass "energy conservation, sampler/pdf agreement"
	else
		fail "BSDF checks failed"
		echo "$out" | grep "FAIL"
	fi
else
	fail "furnace binary not found -- cmake --build build --target furnace"
fi

# --- 6. scene furnace -----------------------------------------------------
# An albedo-1 sphere in a uniform environment must be invisible: every
# pixel equals the environment radiance. Uniform => min == max. Rendered
# at radiance 1 (the criterion as written) and 0.5 (headroom, so a
# too-bright result is not hidden by clipping at 255).
echo "scene furnace"
STAT="$(dirname "$RT")/pngstat"
[ -x "$STAT" ] || STAT="${STAT}.exe"
if [ -x "$STAT" ] && "$RT" tests/scenes/furnace.lua > /dev/null 2>&1; then
	check_uniform() {
		got=$("$STAT" "tests/out/$1.png" 2>&1)
		if [ "$got" = "min $2 max $2" ]; then
			pass "$1: sphere invisible against the environment ($got)"
		else
			fail "$1: expected uniform $2, got '$got'"
		fi
	}
	check_uniform furnace_full 255
	check_uniform furnace_half 128
else
	fail "furnace scene did not render (need the pngstat target)"
fi

echo
if [ "$fails" -eq 0 ]; then
	echo "all tests passed"
else
	echo "$fails test(s) failed"
fi
exit "$fails"
