# Raytracer — status and roadmap

The plan is a **staircase**: each step has an exit criterion you can point at
and say "done". Do not start step N+1 until step N's criterion passes.

---

## 0. Working commands

Standalone repo — every dependency is vendored under `third_party/`.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"      # MinGW, on Windows

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
```

Scenes resolve `Assets/...` relative to the working directory, so run from the
repo root:

```bash
./build/raytracer Assets/simple.lua             # 5 spheres, fast smoke test
./build/raytracer Assets/macho-cows.lua         # ~35k triangles
./build/raytracer prAssets/final_animation.lua  # 85-frame animation
```

Verification and tooling:

```bash
A4_BVH_VERIFY=1 ./build/raytracer Assets/hier.lua   # BVH vs linear scan, every ray
scripts/stitch_animation.sh Renders/bkeytest_frame_ 24 animation.mp4
```

Regression tests:

```bash
tests/run_tests.sh                    # or pass a path to another binary
```

Sources are listed explicitly in `CMakeLists.txt` rather than globbed, so a
new file must be added there — it will fail to link rather than silently not
build.

---

## 1. Where the code stands today

A working **Whitted** ray tracer: recursive specular reflection, direct
lighting from point lights, hard shadows.

- Blinn-Phong shading (ambient + diffuse + specular via half-vector)
- Primitives: spheres, boxes, OBJ triangle meshes
- Hierarchical scene graph — rays into local space, normals by inverse-transpose
- Multithreaded — 16 threads, static scanline bands
- Lua scene description; keyframe animation driven from CSV; PNG output
- Standalone build: no OpenGL, deps vendored

**Fixed already:** the 372× background-by-value copy (28,316 ms → 76 ms,
byte-identical); per-ray heap allocation in `Sphere`/`Cube`/`NonhierBox`;
per-thread `std::mt19937` replacing shared `rand()`; the DoF/AA double-count
that the `.1 *` fudge factor was hiding.

**Scaffolded, core unwritten** — each stub is conservative, so the renderer
stays correct while they are stubs:

| Core | File | Inert behaviour | Staircase step |
|---|---|---|---|
| `sampleUnitDisk()` | `src/render/Sampling.hpp` | returns (0,0), a pinhole | 5 |
| `thinLensRay()` | `src/render/Camera.hpp` | returns the pinhole ray | 5 |
| `AABB::hit()` | `src/geometry/AABB.hpp` | returns `true`, never culls | 8 |
| `BVH::build()` | `src/geometry/BVH.cpp` | leaves `m_built` false | 8 |
| `BVH::traverse()` | `src/geometry/BVH.cpp` | only called once built | 8 |

---

## 2. Step 0 — unblock (before step 1)

Not part of the staircase proper, but step 1 changes the render loop and step 2
changes write-out, so fix these while that code is already open.

- [x] **Resolution is capped at 920×891 and segfaults above it.** Fixed:
      normalised UV mapping, cover-scaled and clamped. Renders at 2048×2048.
      Original description: The
      background center-crop in `rayTraceRGB` indexes outside the decoded PNG
      when the render is larger than the texture:
      `offsetWidthIdx = bgWidthMid - cropWidthMid` goes negative, unchecked.
      Verified: 512×512 fine, 920×920 and 1024×1024 both segfault. You cannot
      produce a high-resolution render until this is fixed, and the background
      is about to be replaced by an environment light in step 3 anyway.
- [x] **Portability: the code only built under GCC.** MSVC failed on the
      `and` / `or` alternative tokens and on `polyroots.cpp` redefining `cbrt`
      (MSVC declares it dllimport, so redefinition is a hard error). Both now
      fixed; the tree builds warning-free under GCC and MSVC, which produce
      byte-identical renders. Visual Studio's sampling profiler is therefore
      available for step 8's writeup.
      *(Correction: `uint` was not an MSVC blocker as first diagnosed -- it was
      a project typedef in `Image.hpp`, not a MinGW type. It has been removed
      anyway, since a project-wide `uint` collides with the POSIX one.)*
- [x] **Latent: children of a `GeometryNode` are transformed twice.** Fixed:
      the transform is applied once via shared `toLocal`/`toWorld`, and child
      traversal is factored into `hitChildren()`. Regression test added in
      `tests/`. Original description:
      `GeometryNode::isHit` builds `localRay`, then hands it to
      `SceneNode::isHit`, which applies the same inverse again — and both
      restore on the way out. `SceneNode::isHit` also overwrites the hit
      material with the geometry node's own, clobbering a child's. No current
      scene nests under a geometry node, so nothing is visibly wrong yet.

---

## 3. The staircase

> **Note on shape.** Step 3 is the pivot. Steps 1–2 are restructuring the
> renderer you have. From step 3 onward you are building a **path tracer** —
> `sample`/`eval`/`pdf`, the furnace test, next event estimation and MIS are
> all path-tracing machinery, and the current `PhongMaterial` (ad-hoc
> `kd`/`ks`/shininess, not energy-conserving, no pdf) gets replaced rather than
> extended. Steps 4, 10 and 11 all depend on that interface existing. Worth
> knowing before you start, so it doesn't feel like scope creep when you get
> there.

### Step 1 — Pixel jitter + accumulation buffer

Jitter the ray inside the pixel footprint. Keep a running radiance sum per
pixel and a sample count; divide at write-out. Restructure the render loop from
"loop N samples then output" to "accumulate forever, snapshot anytime".

**Done when:** edges are smooth, and you can dump an image at any sample count
without re-rendering.

*Where you stand:* the sample loop was restructured recently and is now a
single loop divided once at the end — but it is still "loop N then output".
`gr.set_aa(n)` already jitters within the pixel. `Image` stores `double` per
channel, which is fine as an accumulator. What is missing is the persistent
sum + count and the decoupling of snapshot from completion.

### Step 2 — Linear color pipeline

Radiance stays linear internally. sRGB transfer applied only at write-out. Tone
mapping as a separate, swappable stage (Reinhard now).

**Done when:** you can disable tone mapping and see a raw linear dump, and a
0.5 albedo surface under a 1.0 light reads as 0.5 in linear, not 0.73.

*Where you stand:* nothing exists. `Image::savePng` clamps to [0,1] and writes
8-bit directly — no transfer function, no tone mapping. So everything rendered
so far has been linear values displayed as if they were sRGB, which is why the
images read dark.

### Step 3 — BSDF interface + furnace test

Define the interface before you have many materials:
`sample(wo, rng) -> {wi, throughput, pdf}`, `eval(wo, wi)`, `pdf(wo, wi)`.
Port your existing diffuse to it. Then build the furnace test: uniform emissive
environment of radiance 1, albedo-1 diffuse sphere.

**Done when:** the sphere is invisible against the background. If it's darker,
you're losing energy; brighter, you're double-counting. Fix it before moving on.

*Where you stand:* `Material` is an empty base; `PhongMaterial` holds `kd`,
`ks`, `shininess` and is evaluated inline in `rayTraceRGB`. There is no
sampling, no pdf, no separation between BSDF and integrator. The furnace test
also needs a uniform emissive environment — the current background is a PNG
sampled with a `* 0.3` intensity hack, which will have to go.

### Step 4 — Refraction and reflection

Dielectrics (Snell, TIR, Schlick), smooth metal, rough metal. All through the
interface from step 3.

**Done when:** a glass sphere shows caustics and correct TIR at grazing angles,
and the furnace test still passes with a rough metal sphere at albedo 1.

*Where you stand:* absent. Reflection today is a fixed `glm::mix` at 0.25 over
3 bounces, not a BSDF. Note `Assets/test.lua` already calls `gr.material` with
six arguments — you were reaching for this before.

### Step 5 — Thin-lens camera

Sample a point on the aperture disk, aim through the focal plane. Expose
aperture radius and focus distance.

**Done when:** you can rack focus between a near and far sphere.

*Where you stand:* **scaffolded and ready.** `src/render/Sampling.hpp::sampleUnitDisk()`
and `src/render/Camera.hpp::thinLensRay()` are stubbed with the math spelled out;
`gr.set_lens(aperture, focus, samples)` is already wired through Lua. Two
functions to write. The three bugs in the old attempt are documented in
`src/render/Camera.hpp`: quarter-disk sampling, world-axis offset instead of the camera
basis, and using raw `dirVec.z` instead of a plane intersection.

### Step 6 — Multithreading over tiles

Tile-based job queue, per-thread RNG state (never share a generator), one
shared accumulation buffer.

**Done when:** output is bit-identical to single-threaded at a fixed seed, and
you've measured scaling across thread counts. Expect it to be sublinear — find
out why.

*Where you stand:* per-thread `std::mt19937` seeded by thread index is done.
Decomposition is still static scanline bands, which gives every thread an equal
number of *rows*, not an equal amount of *work*.

Part of "find out why" is already answered, so don't re-derive it: 16 threads
were measured at only **2.05×** on a 20-core machine. The dominant cause was
memory bandwidth — every ray copied the 3.3 MB background — and that is fixed.
**Re-measure from scratch before drawing conclusions.** What remains is band
load imbalance, which is exactly what tiles fix.

### Step 7 — Triangle meshes + glTF

cgltf or tinygltf. Triangle intersection (Möller–Trumbore), vertex normals with
interpolation, transform hierarchies flattened to world space.

**Done when:** a real model renders correctly, and you've recorded the frame
time. You need this number for step 8.

*Where you stand:* OBJ meshes work; the triangle test is the textbook Cramer's
rule formulation, which already computes beta/gamma and throws them away — you
need those barycentrics for interpolation, so they are half the work already
done. Missing: any glTF loader, `vn` parsing (meshes are flat-shaded today),
and flattening (the graph is walked per ray, transforming rays into local space
rather than geometry into world space).

### Step 8 — BVH

SAH construction, flattened to a linear array, iterative traversal.

**Done when:** you can state rays/sec before and after on the same scene, and
explain where the remaining time goes. This is your first serious profiling
writeup.

*Where you stand:* scaffolded. `BVHNode` is already a linear `std::vector` with
integer child indices, so "flattened to an array" is the layout you inherit.
`AABB::surfaceArea()` is there for SAH.

Ordering that makes it debuggable — do **not** skip the checkpoint:

- [ ] `BVH::build()` with a **median split** first (`std::nth_element` on
      centroid bounds, longest axis; guard coincident centroids or it recurses
      forever)
- [ ] **Checkpoint:** with `AABB::hit()` still returning `true`, the image must
      be unchanged and the speed roughly the same. This proves the tree
      contains every triangle before culling exists to hide a bug.
- [ ] `AABB::hit()` slab test — pass a precomputed `1/dir`, and do **not**
      normalize the direction; the rest of the renderer carries unnormalized
      directions and `t` must mean the same thing everywhere
- [ ] `BVH::traverse()` — explicit stack, and **tighten `tBest` on every
      accepted hit**; that is where most of the speedup comes from
- [ ] Front-to-back ordered traversal, then upgrade the split to **SAH** — the
      exit criterion asks for SAH, median is the stepping stone
- [ ] Tune `LEAF_SIZE` (currently 4) and measure

Verify with `A4_BVH_VERIFY=1`, which runs both paths on every ray. A BVH that
is merely slow still renders correctly; one that drops triangles makes holes
that are easy to miss by eye.

### Step 9 — Textures

Procedural checker first — it makes UV seams and winding errors visible
instantly. Then image textures with bilinear sampling. Normal maps last.

**Done when:** a textured glTF model matches a reference render, and you
understand why your first normal map attempt looked wrong.

*Where you stand:* nothing. No `vt` parsing, no UV in the hit record, no
sampler. lodepng is already vendored, so image loading is solved.

### Step 10 — Emissive geometry + next event estimation

Area lights, then explicit light sampling with shadow rays and area-to-solid-
angle PDF conversion.

**Done when:** a small bright light converges in a fraction of the samples it
used to, with the same converged result. Graph both.

*Where you stand:* `Light` is a point-light struct — position, colour, and a
`falloff[3]` that is parsed from Lua and **never read by the shader**. Shadow
rays exist but pass `MAX_T` as the far bound, so geometry *behind* the light
casts shadows; the light sits at `t = 1` since the direction is
`lightPos - hitPoint`. Fix that when you rewrite this path.

### Step 11 — Multiple importance sampling

Combine BSDF and light sampling with the power heuristic.

**Done when:** the furnace test still passes, and rough metal under a large
area light shows no fireflies or dark bands.

*Where you stand:* depends entirely on steps 3 and 10.

### Step 12 — Instancing + motion blur

Transform-instanced geometry sharing one BVH, rays carrying time, transforms
interpolated over the shutter interval. Many instanced spheres with
per-instance motion.

**Done when:** an animated multi-frame sequence renders with correct blur.

*Where you stand:* better than you might expect. `Assets/instance.lua` already
reuses a shared subtree under several parent transforms, so scene-graph
instancing works. The animation pipeline exists — `prAssets/final_animation.lua`
drives 85 CSV keyframes through `gr.render` and `scripts/stitch_animation.sh`
turns the frames into a video. Missing: time on the ray, transform
interpolation, and a shared-BVH instancing path.

`Assets/test.lua` has a commented-out `gr.nh_sphere_mb` carrying a velocity
vector — that was the original idea.

---

## 4. Baselines

Release build, 20 logical cores, wall clock including process start and
decoding the 3.3 MB background texture.

| Scene | Resolution | Time |
|---|---|---|
| `Assets/simple.lua` (5 spheres) | 256×256 | ~120 ms |
| `Assets/macho-cows.lua` (~35k triangles) | 256×256 | ~4.0 s |
| `prAssets/final_animation.lua`, one frame | 512×512 | ~230 ms |
| full 85-frame animation | 512×512 | ~20 s |

The cow scene is roughly **33× slower** than the sphere scene at the same
resolution, because every ray tests every triangle. That gap is step 8's job.

Steps 6, 7 and 8 all ask for numbers. Record them here as you go, alongside the
scene, resolution, sample count and thread count — a rays/sec figure without
those is not comparable to anything.

---

## 5. Off-staircase backlog

Not on the staircase, but cheap and worth folding in when you are next in the
relevant file.

- [ ] **Light falloff** — `Light::falloff` is parsed and never read. Subsumed
      by step 10, but a two-line win before then.
- [ ] **Thread count** is hardcoded to 16; the machine has 20 cores. Use
      `std::thread::hardware_concurrency()`. Fold into step 6.
- [ ] **Background filename** is hardcoded to `"kh_stain_glass.png"` in
      `A4.cpp`; should be a scene parameter. Subsumed by step 3's environment
      light.
- [x] **`RayTracer` is a `Ray`** — renamed. Making the getters `const` (and
      so the whole `isHit` chain, removing the `const_cast` in `Sphere::isHit`)
      is still open.
- [ ] **`Primitive::isHit` returns `false`** instead of being pure virtual, so
      a primitive that forgets to override it silently renders nothing.
- [ ] **`NonhierBox` builds a 12-triangle mesh** per box. A box *is* an AABB —
      once step 8's slab test exists, boxes get an analytic intersection free.
- [ ] **Two different `EPS`** — `1e-6` in `src/render/Renderer.cpp`, `1e-5` in
      `src/geometry/Mesh.cpp`.
- [ ] **`using namespace std/glm` in three headers** — `RayTracer.hpp`,
      `Primitive.hpp`, `HitRecord.hpp` — leaking into every translation unit.
- [ ] **`Image` stores 3 `double`s per pixel** (24 bytes; 25 MB at 1024²).
      Revisit at step 1, when the accumulation buffer is designed.
- [ ] **`JointNode`** is A3 vestigial — bound as `gr.joint`, no `isHit`
      override, unused by any scene.
- [ ] **`RENDER_BOUNDING_VOLUMES`** is a compile-time define branched on at
      runtime in the hot path.
- [x] **`premake4.lua`** removed — the old build system, redundant now CMake
      works.
- [ ] Root clutter: `sample.lua` sits at root while every other scene is in
      `Assets/`.
- [ ] ~~`A4.cpp` / `A4.hpp` are the renderer now~~ — renamed to `src/render/Renderer.*`.

**Open question:** step 7 introduces glTF, which overlaps with what the Lua
scene layer does today. Decide then whether Lua stays as the scene/animation
driver with glTF only for geometry, or whether glTF takes over. The animation
pipeline currently depends on Lua.

Deliberately kept: `polyroots.cpp` is 1079 lines of which only
`quadraticRoots` is called, but its cubic and quartic solvers are most of the
work for torus and cone primitives.

---

## 6. Repo notes

- **Extracted from the CS488 coursework repo.** This was `A4/` inside
  `IbukunSanni/computer-graphics-portfolio`; `git subtree split` preserved all
  31 commits of its history.
- **Standalone build.** The renderer is CPU-only and never needed OpenGL — the
  old build linked it against the course framework (and so GLFW, ImGui and
  OpenGL) for a single 19-line header, `MathUtils.hpp`. That header now lives
  here and the GL stack is gone entirely.
- **Vendored** under `third_party/`: glm, lodepng, Lua 5.3.1.
- **Sources are grouped under `src/`** by concern (core, math, geometry,
  scene, render, lua), with includes written relative to `src/`. Moved with
  `git mv`, so `git log --follow` still works.
- **Warning-free** under GCC (`-Wall`) and MSVC (`/W3`); both produce
  byte-identical renders.
- No top-level licence chosen yet — see the README's provenance note.

