# Raytracer — reference

The detail behind the [README](../README.md): logging, the scene format, the
source layout, tests, style, performance, and why two compilers render
almost but not exactly alike.

## Portability between compilers

Builds warning-free under both GCC/MinGW and MSVC, and the two agree on eight
of the ten scenes in the verification set, byte for byte.

They did not used to. `Rng` drew through `std::uniform_real_distribution`,
whose *algorithm* the standard never specifies — only what it returns. The
engine is portable, the distribution is not, so libstdc++ and MSVC's STL
produced different sequences from an identically seeded `std::mt19937` and the
two builds took different sample paths. `Rng::Next` now does the transform
itself: take the top 24 of mt19937's 32 bits, which are exactly a float's
mantissa, and scale by 2⁻²⁴ — exact, so nothing rounds and the result can
never reach 1. On `simple.lua` at 256×256 that took the disagreement from
**8.07% of pixels (max channel delta 255) to 0.154% (max 173)**.

What is left is not the same class of problem. Eight scenes now match exactly;
`simple.lua` differs in 101 pixels of 65,536 and `nonhier2.lua` in a single
pixel by a delta of 2. Those are the rare draws where a float comparison in
the sampling path lands on either side of a boundary between the two libms, a
rejection is taken by one build and not the other, and that one pixel's
sequence desynchronises. Removing it entirely means removing every
data-dependent float branch from the sampler, which is a different project.

## Animation

`assets/scenes/final_animation.lua` reads keyframes from a CSV, rebuilds the scene
for each frame, and renders a numbered PNG sequence into `renders/`:

```bash
./build/raytracer assets/scenes/final_animation.lua
scripts/stitch_animation.sh renders/test_frames/bkeytest_frame_ 24 animation.mp4
```

The stitching script needs `ffmpeg` on your PATH.

## Logging

Verbosity is controlled by the `RT_LOG` environment variable. No rebuild, no
code change. The default is `info` — three lines per render.

```bash
RT_LOG=off   ./build/raytracer assets/scenes/simple.lua   # silent
RT_LOG=debug ./build/raytracer assets/scenes/simple.lua   # scene, camera, meshes, BVH stats
RT_LOG=trace ./build/raytracer assets/scenes/simple.lua   # every Lua binding call
```

Levels are `off`, `error`, `warn`, `info`, `debug`, `trace`. Categories are
`render`, `scene`, `geom`, `lua`, `image`, and can be set individually as
`category:level`. Entries apply left to right, so a bare level sets a baseline
and later entries override it:

```bash
RT_LOG=off,geom:debug ./build/raytracer assets/scenes/macho-cows.lua
```

```
DEBUG [geom  ] mesh assets/models/cow.obj: 2903 verts, 5804 faces, bvh built
DEBUG [geom  ] bvh frame totals: traversal iterative, nodes visited 4193917, triangles tested 551481
```

Set it for a whole shell session with `export RT_LOG=debug`, or in PowerShell
`$env:RT_LOG = "debug"`.

`RT_LOG_FILE=run.log` mirrors everything to a file as well as the console.
Rendering a working version and a broken one and diffing the two logs is often
the fastest way to find where they diverge.

Misspelled levels and categories are reported rather than ignored, so you never
silently get the wrong verbosity.

Two notes. `trace` is compiled out of Release builds (`RT_LOG_LEVEL` in
`src/core/log.h` defaults to `debug` when `NDEBUG` is set), because trace is
the level meant to sit in inner loops — use a `RelWithDebInfo` build if you
need it. And each log statement builds its whole line in a local buffer and
writes once, so lines from the 20 render threads never interleave.

## Scene format

Scenes are plain Lua, so anything Lua can do — loops, maths, reading a CSV —
is available when building a scene.

**Start from [`assets/scenes/template.lua`](assets/scenes/template.lua)** — a
fully annotated scene that covers materials, the scene graph, transform order,
meshes, lights, sampling and the camera. Copy it and edit.

```lua
matte = gr.lambertian{ kd = {0.7, 0.3, 0.3} }            -- albedo
mat   = gr.blinn_phong{ kd = {0.2, 0.5, 0.2},            -- diffuse
                        ks = {0.5, 0.5, 0.5},            -- specular
                        shininess = 25 }
chrome = gr.mirror{ albedo = {0.9, 0.9, 0.9} }           -- sharp reflection
brushed = gr.metal{ albedo = {0.8, 0.8, 0.8},            -- blurred reflection
                    fuzz = 0.3 }                         -- 0 sharp, 1 widest

scene = gr.node('root')

s1 = gr.nh_sphere('s1', {0, 0, -400}, 100)               -- centre, radius
s1:set_material(mat)
scene:add_child(s1)

s2 = gr.nh_sphere('s2', {-250, 0, -400}, 100)
s2:set_material(matte)
scene:add_child(s2)

key = gr.light({-100, 150, 400}, {2.8, 2.8, 2.8}, {1, 0, 0})

gr.set_background('')                                    -- '' => uniform ambient
gr.set_tonemap{ operator = 'reinhard' }                  -- optional; 'none' by default

gr.render{
  root    = scene,
  output  = 'renders/out.png',
  width   = 512, height = 512,
  eye     = {0, 0, 800},
  view    = {0, 0, -1},        -- a look DIRECTION, not a target point
  up      = {0, 1, 0},
  fov     = 50,                -- vertical, degrees
  ambient = {0.3, 0.3, 0.3},
  lights  = { key },

  -- Optional from here down; omit one and the renderer keeps its default.
  samples       = 64,          -- per pixel
  max_depth     = 8,           -- bounce cap; raise it for glass
  defocus_angle = 2.9,         -- degrees; 0 (default) is a pinhole
  focus_dist    = 800.0,       -- required alongside defocus_angle
  lens_samples  = 16,
}
```

`gr.render` takes a named table. Missing or misspelled fields are errors that
name the field and point at the line, rather than silent defaults.

The original ten-argument positional form still works, so existing scenes did
not have to change:

```lua
gr.render(scene, 'out.png', 512, 512, {0,0,800}, {0,0,-800}, {0,1,0}, 50,
          {0.3,0.3,0.3}, {key})
```

## Architecture

Sources live under `src/`, grouped by concern.

```
src/
  main.cc             entry point
  core/               ray, hit_record, image, log, tone_map, stats
  math/               math_utils, polyroots
  geometry/           primitive, mesh, aabb, bvh
  scene/              scene_node, geometry_node, joint_node, light, material
  render/             renderer (settings, bands, frame loop), integrator
                      (the path tracer), environment, progress,
                      frame_stats, framebuffer, camera, sampling
  lua/                Lua bindings
assets/
  scenes/             .lua scene descriptions
  models/             .obj meshes
  textures/           background / image textures
  animation/          keyframe CSVs
docs/
  images/             figures and renders for the README, docs and posts
posts/                blog drafts -- see posts/README.md
renders/              output (gitignored)
scripts/              style gate, generators, post and video tooling
tests/                regression scenes and runner
third_party/          glm, lodepng, Lua (vendored)
```

Includes are written relative to `src/`, e.g. `#include "geometry/mesh.h"`,
so only `src/` and `third_party/` are on the include path.

## Tests

```bash
cmake --build build && ctest --test-dir build -j 8 --output-on-failure
```

With `-j 8` the suite runs in about 3.5 s (15.8 s serially), and is quiet unless
something fails. It covers the things that have broken before and the ones
that would break silently: a child parented to a `GeometryNode` must render
identically to the same child under a plain node, so the transform is applied
exactly once; rendering must work above the background texture's size; the BVH
must agree with the linear scan on every ray; the materials must conserve
energy and their samplers must agree with their pdfs; and an albedo-1 sphere
in a uniform environment must be invisible.

Two binaries, split by what a failure would mean. `bsdf_test` integrates the
materials directly — no scene, no image, no renderer — so a failure names a
material. `render_test` runs the raytracer on a scene and reads the PNG back,
so a failure could be the integrator or the image writer instead. Every scene
it uses has an output you can predict without rendering it, a flat colour or
a second render that must match byte for byte, so there are no reference
images to keep up to date.

Tolerances in the material checks are four standard errors computed from the
run itself, so a failure means a material is wrong rather than a seed unlucky.

Everything lives under `tests/`: the two test files, the Lua scenes they
render, and `support/` for the assertion and probe helpers they share.
`tests/CMakeLists.txt` is where the targets are declared, and adding a test
case needs no change there — the build discovers them.

Run one test, or one area:

```bash
ctest --test-dir build -R metal -L bsdf/metal
```

`BVH_VERIFY=1` makes every ray run both the BVH and the linear scan and reports
any disagreement — the check that matters while implementing step 8.

## Style

The [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
enforced rather than described. Two config files hold the rules and one script
runs them, so the guide is never a thing you have to remember:

| File | Covers |
|---|---|
| `.clang-format` | Whitespace, braces, line breaks, include order, LF endings |
| `.clang-tidy` | Naming — needs the AST, so it compiles each file |
| `.gitattributes` | Line endings at the git layer |

```bash
scripts/check_style.sh          # report; exits non-zero on a violation
scripts/check_style.sh --fix    # rewrite the formatting
cmake --build build --target style      # the same check, through CMake
cmake --build build --target style-fix
```

Three places catch a violation, in increasing order of how late it is:

- **`.githooks/pre-commit`** — formatting only, on staged files, in under a
  second. Opt in per clone with `git config core.hooksPath .githooks`.
- **`cmake --build build --target style`** — formatting and naming, locally.
- **`.github/workflows/style.yml`** — both, on every push, whether or not
  anyone enabled the hook.

Naming needs clang-tidy to parse the tree, which on Windows means pointing it
at MinGW's headers; the script does that automatically and says `skip` rather
than failing if it cannot. `RT_TIDY_FLAGS` overrides the guess.

Two documented exceptions, both marked in the source with `NOLINT` and a
reason: `src/math/polyroots.cc`, whose one-letter identifiers match the papers
the solvers are transcribed from and would collide if lowercased, and
`StringMaker::convert` in `tests/support/statistics.h`, which doctest looks up
by that exact spelling.

### Names for ray hits

clang-tidy checks the case of a name, not what it means, so this one is held
by hand. **A noun names what was hit; `is_` names whether it was.**

| What it is | Name | Examples |
|---|---|---|
| `bool` from a hit test | `is_hit` | `is_hit`, `ref_is_hit`, `linear_is_hit`, `walk.is_hit` |
| `HitRecord` | `hit` | `hit`, `prim_hit`, `child_hit`, `local_hit`, `ref_hit` |
| `BVHHit` | `*_hit` | `bvh_hit`, `tree_hit`, `out_hit` |
| The test itself | `IsHit()` | |

So `hit.GetT()` is always valid and `if (is_hit)` is always a `bool`. Before
6 October the same suffix meant both: `ref_hit` was a `bool` beside the
`BVHHit` `bvh_hit` in one expression in `Mesh::IsHit`.

### Acronyms keep their capitals

`BVHNode`, `SAHSplit`, `SetUV`, not `BvhNode` or `SetUv`. This departs from
Google, which writes an acronym as a word (`StartRpc`). That is deliberate:
these names are read as letters, and the BVH code used capitals first.
clang-tidy cannot tell the difference, so this one is held by hand too.

Older names that predate the rule: `RayTraceRgb`, `SavePng`, `DecodeSrgb`,
`EncodeSrgb`. Rename them when that file is next open for something else.

### Comments

No tool checks these, so they are the part of the style held by hand.

**A comment earns its place by explaining context, not by restating code.**
`src/core/log.h` opens with three numbered properties — why logging is macros
rather than functions, why the disabled branch is still compiled, why a line is
assembled in a local buffer before one guarded write. None of that is visible
in the declarations below it. That is the bar.

**A comment that describes current state is a comment that will drift.** Two
found on 2026-09-13: `material.cc` still said *"No Fresnel split yet — every
ray transmits"* three lines below the Fresnel split that had just landed, and
`nonhier2.lua` referred to a `nonhier.lua` that had been deleted. Prefer the
comment that stays true — the invariant, the reason, the trap. When a
state-describing comment is genuinely needed, it changes in the same commit as
the code it describes, or it is a bug.

**Comments that guide implementation are the most valuable ones here — and
they expire with the scaffold.** `src/render/camera.h` carried a 45-line block
spelling out the three steps of the thin lens and the three bugs a previous
attempt had made. It earned its keep while the function was a stub and became
dead weight the moment the function existed: the steps had become the code, and
the bug list pointed at a diff nobody can see. Cut to 67 lines from 124 on
22 September, keeping only what the code cannot say — why the focal point
projects onto the view axis instead of dividing by ray length, and that
`SampleUnitDisk` is shared with the BSDF sampler so a shaped aperture cannot go
there. Write scaffolding freely; take it down with the scaffold.

The same rule governs `docs/`. ROADMAP's *where you stand* notes and
ENGINEERING's entries are comments at a larger scale and drift the same way —
a claim is only worth keeping if it is corrected when it stops being true.

## Performance

Measured on a 14-core, 20-thread machine (i7-12700H), Release build, 1 spp. **Render** is what the
renderer logs; **wall** is the whole process, including start-up and decoding
the 3.3 MB background texture — a fixed ~80 ms an accelerator cannot touch.

To see where the render time goes, profile with `scripts/sample_profile.cc`
and `scripts/symbolize_profile.py`; the build and run commands are in the
`.cc` file's header. Time and count in separate runs: `RT_STATS=1` and the
profiler both slow the render they measure.

| Scene | Resolution | Render | Wall |
|---|---|---|---|
| `assets/scenes/simple.lua` (5 spheres) | 256×256 | 16 ms | ~94 ms |
| `assets/scenes/macho-cows.lua` (17.4k triangles) | 256×256 | ~3490 ms | ~3580 ms |
| one animation frame | 512×512 | — | ~230 ms |

The mesh scene is about **218×** slower than the sphere scene at the same
resolution, because every ray currently tests every triangle. That is what the
BVH is for. The ratio used to be quoted as 33×, from dividing the two wall
clocks — but 83% of `simple.lua`'s wall time is start-up, so that comparison
was measuring process launch as much as geometry.

One finding worth recording: the renderer used to spend **432 µs per pixel** on
a five-sphere scene. Profiling showed only 12.5 intersection tests per pixel,
so the cost was never in the intersection maths — `RayTraceRgb` took the
background image *by value*, and it decodes to 3.3 MB, so every ray copied it.
Passing by reference took `simple.lua` at 256×256 from **28,316 ms to 76 ms**
with byte-identical output, and explained why 16 threads had only been buying
2× on 14 cores (20 threads): the renderer was memory-bandwidth-bound, not compute-bound.

