# A4 Raytracer — status and roadmap

Working document. Tick things off as they land.

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
./build/raytracer Assets/macho-cows.lua         # ~35k triangles, the BVH target
./build/raytracer prAssets/final_animation.lua  # 85-frame animation
```

Verification and tooling:

```bash
A4_BVH_VERIFY=1 ./build/raytracer Assets/hier.lua   # BVH vs linear scan, every ray
scripts/stitch_animation.sh Renders/bkeytest_frame_ 24 animation.mp4
```

`CMakeLists.txt` globs `*.cpp` at configure time, so re-run the configure step
after adding a **new** source file.

---

## 1. Done

### 1.1 Renderer (original assignment)

- [x] Whitted recursive ray tracing — 3 bounces, blended with `glm::mix` at 0.25
- [x] Blinn-Phong shading — ambient + diffuse + specular via half-vector
- [x] Hard shadows — one shadow ray per point light
- [x] Primitives — sphere, cube, nonhier sphere/box, OBJ triangle mesh
- [x] Hierarchical scene graph — ray transformed to local space, normals by
      inverse-transpose
- [x] Multithreaded rendering — 16 threads over horizontal bands
- [x] Background image — PNG sampled for miss rays
- [x] Lua scene description — `gr.node`, `gr.sphere`, `gr.mesh`, `gr.light`,
      `gr.render`, …
- [x] PNG output via lodepng

### 1.2 Animation (already built, predates this pass)

- [x] Keyframes in CSV — `prAssets/ball_Control_frame_locations.csv`, 85 rows
- [x] Lua frame loop — `prAssets/final_animation.lua` rebuilds the scene per
      frame and calls `gr.render` to a numbered filename
- [x] 97 rendered frames in `Renders/`
- [x] **Frames to video** — `scripts/stitch_animation.sh` (ffmpeg). This was the
      only genuinely missing piece; the animation itself already worked.

### 1.3 Performance and correctness fixes

- [x] **Background passed by value, now by reference.** `rayTraceRGB` copied the
      3.3 MB decoded background PNG *on every ray*. `simple.lua` at 256×256:
      **28,316 ms to 76 ms (372×)**, output byte-identical.
      This also explained why 16 threads only gave **2.05×** on a 20-core
      machine — the renderer was memory-bandwidth-bound, not compute-bound.
- [x] **Per-ray heap allocation removed.** `Sphere::isHit` and `Cube::isHit`
      allocated *and leaked* a primitive per intersection test;
      `NonhierBox::isHit` built a whole 12-triangle `Mesh` per test. Now built
      once behind `std::call_once`. Critical to do before BVH — otherwise it
      would have become *building a tree per ray*.
- [x] **Sampling loop restructured.** DoF and AA were separate blocks
      accumulating into the same pixel, so DoF-on/AA-off added a full-weight
      sharp sample on top of the blurred average. That is what the
      `.1 *` "constant reduce factor not sure why" was compensating for. Now one
      sample loop, divided by the total exactly once.
- [x] **Thread-safe RNG.** `rand()` shares global state across threads — both a
      contention point and non-reproducible. Now a per-thread `std::mt19937`
      seeded by thread index, so a render repeats run to run.

### 1.4 Infrastructure added

- [x] `gr.set_lens(aperture_radius, focus_distance, samples)` — runtime setting,
      not a `#define`
- [x] `gr.set_aa(samples)` — same; both default to the old behaviour
- [x] `A4_BVH_VERIFY=1` — runs BVH and linear scan on every ray, reports any
      disagreement
- [x] Per-frame BVH stats — nodes visited, triangles tested
- [x] Mesh load reports face count and whether its BVH built

---

## 2. In progress — scaffolded, core left to write

Every stub is **conservative**: it compiles, renders *correctly*, and does
nothing yet. Images stay valid at every step; only speed and blur change as they
get filled in. Verified — `simple.lua` after scaffolding is byte-identical to
before (0 of 196,608 bytes differ).

| Core | File | Inert behaviour |
|---|---|---|
| `sampleUnitDisk()` | `Sampling.hpp` | returns (0,0), so the lens is a pinhole |
| `thinLensRay()` | `Camera.hpp` | returns the pinhole ray unchanged |
| `AABB::hit()` | `AABB.hpp` | returns `true`, so it never culls but stays correct |
| `BVH::build()` | `BVH.cpp` | leaves `m_built = false`, so `Mesh` scans linearly |
| `BVH::traverse()` | `BVH.cpp` | returns no hit; only called once `m_built` |

### 2.1 Depth of field

A pinhole has zero aperture area, so one ray per pixel and everything is sharp.
A real lens has area — all lens points image a given *focal-plane* point to the
same sensor point, so those stay sharp, while nearer and farther points spread
into a circle of confusion that grows with aperture radius. Simulate it by
sampling the lens, not by modelling glass.

- [ ] **`sampleUnitDisk()`** — `Sampling.hpp`. Uniform point on the unit disk.
      Rejection sampling, or polar with `r = sqrt(u2)`. The sqrt is the whole
      trick: without it samples pile up at the centre, because the area of an
      annulus grows with r.
- [ ] **`thinLensRay()`** — `Camera.hpp`. Three steps:
      1. Find the focal point — walk the pinhole ray to the focal plane, scaling
         by `focusDistance / dot(pinDir, cam.wVec)`. **Along the view axis**,
         not along the ray, or the focal surface curves and frame edges focus
         nearer than the centre.
      2. Pick a lens point — `sampleUnitDisk() * apertureRadius`, mapped through
         `cam.uVec` / `cam.vVec`. **The camera basis**, not world axes.
      3. Aim — origin `eye + offset`, direction `focalPoint - newOrigin`.
- [ ] Add a DoF demo scene with objects at three depths

The old attempt had three separate bugs, worth remembering: `randUnitVector()`
drew x and y from `[0,1)`, giving a **quarter** disk rather than a disk; the
offset was applied on world axes; and the focal point came from raw `dirVec.z`
instead of a plane intersection.

**Verify:** aperture 0 must stay pixel-identical to the current render. Then a
scene with objects at three depths — only the one at `focus_distance` sharp.

### 2.2 BVH

Currently O(rays × triangles) — every ray tests every triangle of every mesh. A
BVH wraps subtrees in AABBs so one cheap box test skips a whole subtree, giving
roughly O(rays × log triangles).

Do these **in this order** — it is what makes the thing debuggable:

- [ ] **`BVH::build()`** — `BVH.cpp`. Median split. Keep `m_indices` as a
      permutation so triangles never move; `std::nth_element` for the O(n)
      median; split on the longest axis of the **centroid** bounds, not the
      triangle bounds (a few large triangles would otherwise dominate the
      choice of axis). Guard the degenerate case: coincident centroids can put
      every triangle on one side and recurse forever — make a leaf instead.
- [ ] **Checkpoint.** With `AABB::hit()` still returning `true`, the image must
      be **unchanged** and the speed roughly the same. That proves the tree
      contains every triangle, before any culling exists to hide a bug.
- [ ] **`AABB::hit()`** — `AABB.hpp`. Slab test, taking a precomputed `1/dir`.
      Do **not** normalize the direction — the rest of A4 carries unnormalized
      directions, and the BVH has to agree with the triangle test about what `t`
      means.
- [ ] **`BVH::traverse()`** — `BVH.cpp`. Explicit stack, not recursion.
      **Tighten `tBest` on every accepted hit** — that is where most of the
      speedup actually comes from, because a shrinking `tBest` makes later box
      tests fail far more often.
- [ ] Ordered front-to-back traversal (push the far child first) — only after
      the unordered version is correct
- [ ] Tune `LEAF_SIZE` (currently 4) and measure

**Verify:** `A4_BVH_VERIFY=1` on every scene. A BVH that is merely slow still
renders correctly; one that drops triangles makes holes that are easy to miss by
eye. Watch triangles-tested in the stats line fall as the tree improves.

### 2.3 Animation — what is left

- [ ] **Cache meshes by filename.** `gr.mesh('cow', 'cow.obj')` re-reads and
      re-parses the OBJ on every call. The Lua loop rebuilds the whole scene per
      frame, so that cost is paid 85 times — and once BVHs exist, that is 85
      tree builds too. This is the thing that will hurt when BVH lands.
- [ ] Camera animation — the CSV drives objects; eye, view and focus are fixed.
      Animating focus distance gives a rack focus, the best showcase for DoF.
- [ ] Interpolate between keyframes instead of one CSV row per frame
- [ ] Motion blur — jitter a time value per sample within the shutter interval
      and interpolate transforms. `test.lua` has a commented-out
      `gr.nh_sphere_mb` carrying a velocity vector; that was the idea.

---

## 3. Baselines

Release build, 20 logical cores, after the 372× fix.

| Scene | Resolution | Time |
|---|---|---|
| `Assets/simple.lua` (5 spheres) | 256×256 | ~120 ms |
| `Assets/macho-cows.lua` (~35k triangles) | 256×256 | ~4.0 s |
| `prAssets/final_animation.lua`, one frame | 512×512 | ~230 ms |
| full 85-frame animation | 512×512 | ~20 s (was ~2 hours) |

The cow scene is roughly **33× slower** than the sphere scene at the same resolution.
That gap is the BVH's job. It is the dominant cost now — it was not before the
background fix, which is exactly why measuring first mattered.

---

## 4. Backlog

Roughly ordered by payoff per unit of work.

### Cheap, high impact

- [ ] **Light falloff.** `Light::falloff` is parsed from Lua and stored, and
      never read by the shader. Divide the contribution by
      `f[0] + f[1]*d + f[2]*d*d`. Scenes gain depth immediately.
- [ ] **Shadow ray far bound.** Shadow rays pass `MAX_T`, so geometry *behind
      the light* casts shadows. The direction is `lightPos - hitPoint`, so the
      light sits at `t = 1` — that is the correct bound.
- [ ] **Gamma correction.** `Image::savePng` writes linear values straight into
      8-bit sRGB, so everything is darker than it should be. One `pow(c, 1/2.2)`.

### Shading and materials

- [ ] Refraction and transparency — Snell's law plus Fresnel (Schlick's
      approximation is the usual shortcut). Needs an index of refraction on the
      material; `test.lua` already calls `gr.material` with six arguments, so
      this was already the direction of travel.
- [ ] Soft shadows — give lights area and sample several points on them. Same
      machinery as lens sampling; reuse `sampleUnitDisk`.
- [ ] Smooth normals — the OBJ loader ignores `vn`, so meshes are flat-shaded.
      `isTriangleIntersection` already computes beta and gamma and throws them
      away.
- [ ] Texture mapping — needs `vt` parsing and the same barycentric weights
- [ ] Bump / normal mapping

### Performance beyond the mesh BVH

- [ ] Top-level BVH over scene nodes, not just triangles within a mesh — makes
      many-object scenes and instancing cheap
- [ ] SAH splits instead of median — typically about 2× better trees for more
      build time. `AABB::surfaceArea()` is already there for it.
- [ ] Dynamic tile queue instead of static horizontal bands. Bands give every
      thread an equal number of *rows*, not an equal amount of *work* — a thread
      that draws only background finishes instantly and then idles.
- [ ] Adaptive sampling — more rays only where variance is high

### Bigger jumps

- [ ] Path tracing / global illumination — cosine-weighted hemisphere sampling,
      Russian roulette. A different renderer, not an increment.
- [ ] CSG — union, intersection, difference
- [ ] Quadrics beyond the sphere — cone, cylinder, torus. `polyroots.cpp`
      already has cubic and quartic solvers sitting unused.

---

## 5. Repo notes

- **Extracted from the CS488 coursework repo.** This was `A4/` inside
  `IbukunSanni/computer-graphics-portfolio`; `git subtree split` preserved all
  31 commits of its history.
- **Standalone build.** The renderer is CPU-only and never needed OpenGL — the
  old build linked it against the course framework (and so GLFW, ImGui and
  OpenGL) for a single 19-line header, `MathUtils.hpp`. That header now lives
  here and the GL stack is gone entirely.
- **Vendored** under `third_party/`: glm, lodepng, Lua 5.3.1.
- No top-level licence chosen yet — see the README's provenance note.
- Possible tidy-up: move sources into `src/`. Deferred because it would make
  `git log` harder to follow across the rename.
