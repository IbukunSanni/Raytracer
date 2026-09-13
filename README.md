# Raytracer

A multithreaded CPU ray tracer in C++, with scenes described in Lua.

![A rendered scene: low-poly trees, spheres and a sun over a green plane](docs/images/sample.png)

No OpenGL, no windowing, no GUI — it reads a Lua scene, traces it across
every core, and writes a PNG.

## Features

- **Path tracing** — an iterative throughput walk, terminated by Russian
  roulette rather than a fixed bounce count
- **Energy-conserving BSDFs** — `Eval` / `Pdf` / `Sample` behind one
  interface: Lambertian, and a normalised `(n+2)/8π` Blinn-Phong with
  luminance-weighted two-lobe importance sampling
- **Verified by furnace test** — the BSDFs are integrated in isolation, and an
  albedo-1 sphere in a uniform environment must render invisible
- **Image-based lighting** — a lat-long environment map sampled by ray
  direction, so the background lights the scene rather than just backing it
- **Hard shadows** — one shadow ray per point light, as crude next event
  estimation; point lights are Dirac deltas that BSDF sampling cannot reach
- **Primitives** — spheres, boxes, and arbitrary triangle meshes from OBJ files
- **Hierarchical scene graph** — rays are transformed into each node's local
  space; normals are carried back by the inverse-transpose
- **Multithreaded** — the image is split across threads by scanline band
- **Progressive accumulation** — every sample is jittered inside the pixel
  footprint and added to a running buffer, so the image can be snapshotted at
  any sample count without re-rendering
- **Linear colour pipeline** — radiance stays linear through shading and
  accumulation; the tone map and sRGB transfer are applied once, at write-out
  (`gr.set_tonemap`)
- **Keyframe animation** — a Lua loop drives per-frame transforms from a CSV
  and renders a numbered PNG sequence
- **Lua scene description** — geometry, materials, lights and camera

In progress: depth of field via a thin-lens camera, and a BVH. See
[docs/ROADMAP.md](docs/ROADMAP.md).

New to the code? [docs/WALKTHROUGH.md](docs/WALKTHROUGH.md) follows a single ray
from `main()` to a byte in a PNG.

## Building

Needs a C++17 compiler and CMake 3.16+. Every dependency is vendored under
`third_party/` (glm, lodepng, Lua), so there is nothing to install.

Builds warning-free under both GCC/MinGW and MSVC.

The two do **not** produce byte-identical renders, though, and the reason is
worth knowing: `Rng` draws through `std::uniform_real_distribution`, whose
algorithm the standard leaves to the implementation. libstdc++ and MSVC's STL
therefore return different sequences from an identically seeded `std::mt19937`,
so the two builds take different sample paths. Measured on
`assets/scenes/simple.lua` at 256×256: 8.1% of pixels differ, some by a full
channel — Monte Carlo noise, not drift. Replacing the distribution with an
explicit transform of the engine output would make the two agree, at the cost
of changing every render this project has produced so far.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On Windows with MSYS2/MinGW:

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Running

Scenes resolve asset paths relative to the working directory, so run from the
repo root:

```bash
./build/raytracer assets/scenes/simple.lua        # five spheres
./build/raytracer assets/scenes/macho-cows.lua    # ~35k triangles
./build/raytracer                          # defaults to assets/scenes/simple.lua
```

The output filename, resolution and camera all come from the scene's
`gr.render` call.

### Animation

`assets/scenes/final_animation.lua` reads keyframes from a CSV, rebuilds the scene
for each frame, and renders a numbered PNG sequence into `renders/`:

```bash
./build/raytracer assets/scenes/final_animation.lua
scripts/stitch_animation.sh renders/bkeytest_frame_ 24 animation.mp4
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
DEBUG [geom  ] mesh assets/models/cow.obj: 2903 verts, 5804 faces, bvh not built (linear scan)
DEBUG [geom  ] bvh frame totals: nodes visited 0, triangles tested 0
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

gr.set_samples(64)                                       -- samples per pixel
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
  core/               ray, hit_record, image, log, tone_map
  math/               math_utils, polyroots
  geometry/           primitive, mesh, aabb, bvh
  scene/              scene_node, geometry_node, joint_node, light, material
  render/             renderer, framebuffer, camera, sampling
  lua/                Lua bindings
assets/
  scenes/             .lua scene descriptions
  models/             .obj meshes
  textures/           background / image textures
  animation/          keyframe CSVs
docs/
  images/             README artwork
  reference/          expected-output renders
renders/              output (gitignored)
tests/                regression scenes and runner
third_party/          glm, lodepng, Lua (vendored)
```

Includes are written relative to `src/`, e.g. `#include "geometry/mesh.h"`,
so only `src/` and `third_party/` are on the include path.

## Tests

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

The suite runs in a couple of seconds, in parallel, and is quiet unless
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

## Performance

Measured on a 20-core machine, Release build, wall clock including process
start and decoding the 3.3 MB background texture.

| Scene | Resolution | Time |
|---|---|---|
| `assets/scenes/simple.lua` (5 spheres) | 256×256 | ~120 ms |
| `assets/scenes/macho-cows.lua` (~35k triangles) | 256×256 | ~4.0 s |
| one animation frame | 512×512 | ~230 ms |

The mesh scene is about 33× slower than the sphere scene at the same
resolution, because every ray currently tests every triangle. That is what the
BVH is for.

One finding worth recording: the renderer used to spend **432 µs per pixel** on
a five-sphere scene. Profiling showed only 12.5 intersection tests per pixel,
so the cost was never in the intersection maths — `RayTraceRgb` took the
background image *by value*, and it decodes to 3.3 MB, so every ray copied it.
Passing by reference took `simple.lua` at 256×256 from **28,316 ms to 76 ms**
with byte-identical output, and explained why 16 threads had only been buying
2× on 20 cores: the renderer was memory-bandwidth-bound, not compute-bound.

## Provenance and licensing

This began as assignment 4 of the University of Waterloo's CS488 computer
graphics course and has been extended well beyond it. Some scaffolding
(`polyroots.cc`, the Lua binding layer, `Image`) comes from the
course-provided skeleton.

Vendored third-party code keeps its own licence: glm (MIT), lodepng (zlib),
Lua (MIT). No top-level licence has been chosen for the original work yet.
