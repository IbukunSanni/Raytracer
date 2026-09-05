# Raytracer

A multithreaded CPU ray tracer in C++, with scenes described in Lua.

![A rendered scene: low-poly trees, spheres and a sun over a green plane](docs/images/sample.png)

No OpenGL, no windowing, no GUI — it reads a Lua scene, traces it across
every core, and writes a PNG.

## Features

- **Whitted recursive ray tracing** — reflections to a configurable depth
- **Blinn-Phong shading** — ambient, diffuse and specular via the half-vector
- **Hard shadows** — one shadow ray per point light
- **Primitives** — spheres, boxes, and arbitrary triangle meshes from OBJ files
- **Hierarchical scene graph** — rays are transformed into each node's local
  space; normals are carried back by the inverse-transpose
- **Multithreaded** — the image is split across threads by scanline band
- **Progressive accumulation** — every sample is jittered inside the pixel
  footprint and added to a running buffer, so the image can be snapshotted at
  any sample count without re-rendering
- **Linear colour pipeline** — radiance stays linear through shading and
  accumulation; a swappable tone map (Reinhard) and the sRGB transfer are
  applied once, at write-out (`gr.set_tonemap`)
- **Keyframe animation** — a Lua loop drives per-frame transforms from a CSV
  and renders a numbered PNG sequence
- **Lua scene description** — geometry, materials, lights and camera

In progress: depth of field via a thin-lens camera, and a BVH. See
[docs/ROADMAP.md](docs/ROADMAP.md).

New to the code? [docs/WALKTHROUGH.md](docs/WALKTHROUGH.md) follows a single ray
from `main()` to a byte in a PNG.

## Building

Needs a C++14 compiler and CMake 3.16+. Every dependency is vendored under
`third_party/` (glm, lodepng, Lua), so there is nothing to install.

Builds warning-free under both GCC/MinGW and MSVC, which produce byte-identical
renders.

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
`src/core/Log.hpp` defaults to `debug` when `NDEBUG` is set), because trace is
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
mat = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25)  -- diffuse, specular, shininess

scene = gr.node('root')

s1 = gr.nh_sphere('s1', {0, 0, -400}, 100)               -- centre, radius
s1:set_material(mat)
scene:add_child(s1)

key = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})

gr.set_samples(64)                                       -- samples per pixel
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
  main.cpp            entry point
  core/               Ray, HitRecord, Image
  math/               MathUtils, polyroots
  geometry/           Primitive, Mesh, AABB, BVH
  scene/              SceneNode, GeometryNode, JointNode, Light, materials
  render/             Renderer, Framebuffer, Camera, Sampling
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

Includes are written relative to `src/`, e.g. `#include "geometry/Mesh.hpp"`,
so only `src/` and `third_party/` are on the include path.

## Tests

```bash
tests/run_tests.sh
```

Covers three things that have broken before: a child parented to a
`GeometryNode` must render identically to the same child under a plain node
(the transform must be applied exactly once); rendering must work above the
background texture's size; and the BVH must agree with the linear scan on
every ray. Pass a different binary as the first argument to test another build.

`BVH_VERIFY=1` makes every ray run both the BVH and the linear scan and reports
any disagreement — the check that matters while implementing step 8.

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
so the cost was never in the intersection maths — `rayTraceRGB` took the
background image *by value*, and it decodes to 3.3 MB, so every ray copied it.
Passing by reference took `simple.lua` at 256×256 from **28,316 ms to 76 ms**
with byte-identical output, and explained why 16 threads had only been buying
2× on 20 cores: the renderer was memory-bandwidth-bound, not compute-bound.

## Provenance and licensing

This began as assignment 4 of the University of Waterloo's CS488 computer
graphics course and has been extended well beyond it. Some scaffolding
(`polyroots.cpp`, the Lua binding layer, `Image`) comes from the
course-provided skeleton.

Vendored third-party code keeps its own licence: glm (MIT), lodepng (zlib),
Lua (MIT). No top-level licence has been chosen for the original work yet.
