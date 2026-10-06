# Raytracer — status and roadmap

The plan is a **staircase**: each step has an exit criterion you can point at
and say "done". Do not start the next step until the current one's criterion
passes. Scope and dates live in `ENGINEERING.md`; this file says what each step
is, how it is scored, what was measured and why, and what went wrong on the
way.

**How each step is laid out.** Every step opens with an *at a glance* table
(goal, exit criterion, status, headline result), then an *issues and fixes*
table you can scan in ten seconds, then the detail: what was built, what was
measured and why, and the full story of each issue. Read the tables to know
where things stand; read the detail when writing a post or touching the code.

---

## Status

**Now:** step 9, piece 1a — pass the `HitRecord` into the BSDF. (The
benchmark trim before it, step 8 rung E, is done.)
**Deadline:** 10 October 2026 (moved twice; see `ENGINEERING.md`).

| Step | Topic | Status | Headline |
|---|---|---|---|
| 0 | [Unblock](#3-step-0--unblock) | ✅ | 2048² renders; builds under GCC and MSVC |
| 1 | [Pixel jitter + accumulation](#step-1--pixel-jitter--accumulation-buffer) | ✅ | Snapshot at any spp; RMS 0.820 → 0.083 |
| 2 | [Linear colour pipeline](#step-2--linear-color-pipeline) | ✅ | Raw linear dump; swappable tone map |
| 3 | [BSDF interface + furnace](#step-3--bsdf-interface--furnace-test) | ✅ | Albedo-1 sphere invisible in the furnace |
| 4 | [Refraction and reflection](#step-4--refraction-and-reflection) | ✅ 13 Sep | Caustic core **1.98×** the floor |
| 5 | [Thin-lens camera](#step-5--thin-lens-camera) | ✅ 22 Sep | In focus **0.99×** pinhole sharpness, out of focus 0.32× |
| 8 | [BVH](#step-8--bvh) | 🔨 core done 5 Oct | `macho-cows` **4,725 → 23.2 ms (204×)**; write-up items open |
| 9 | [Textures](#step-9--textures) | 🔨 started 6 Oct | Piece 1a next |
| 12 | [Motion blur](#step-12--instancing--motion-blur) | ◇ stretch | — |
| 6, 7, 10, 11 | [Deferred](#5-deferred-past-the-deadline) | ⏸ | Tiles, glTF, NEE, MIS |

✅ done · 🔨 in progress · ◇ stretch goal · ⏸ after the deadline

## Contents

1. [Working commands](#1-working-commands)
2. [The renderer today](#2-the-renderer-today)
3. [Step 0 — unblock](#3-step-0--unblock)
4. [The staircase](#4-the-staircase): steps 1, 2, 3, 4, 5, 8, 9, 12
5. [Deferred past the deadline](#5-deferred-past-the-deadline): steps 6, 7, 10, 11, tools
6. [How measurements are taken](#6-how-measurements-are-taken), and the old baselines
7. [Off-staircase backlog](#7-off-staircase-backlog)
8. [Repo notes](#8-repo-notes)

---

## 1. Working commands

Standalone repo — every dependency is vendored under `third_party/`.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"      # MinGW, on Windows

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
```

Scenes resolve `assets/...` relative to the working directory, so run from the
repo root:

```bash
./build/raytracer assets/scenes/simple.lua             # 5 spheres, fast smoke test
./build/raytracer assets/scenes/macho-cows.lua         # 17.4k triangles
./build/raytracer assets/scenes/final_animation.lua    # 85-frame animation
./build/raytracer assets/scenes/caustic.lua            # sun through a glass ball
```

**Verification and debugging:**

```bash
BVH_VERIFY=1 ./build/raytracer assets/scenes/hier.lua           # BVH vs linear scan, every ray
BVH_TRAVERSAL=linear ./build/raytracer ...                      # linear | recursive | iterative
BVH_SPLIT=sah ./build/raytracer ...                             # median | sah
RT_STATS=1 RT_LOG=off,geom:debug ./build/raytracer ...          # counts: rays, nodes, triangles
RT_THREADS=1 ./build/raytracer ...                              # one thread: ordered logs
RT_VIEW=normal ./build/raytracer ...                            # normals as RGB, no shading
scripts/bench_bvh.sh -s "median sah" -p 4 cornell_box rtiow_final   # ~1 min
RT_SPP=4 ./build/raytracer ...                                  # override samples per pixel
scripts/stitch_animation.sh renders/test_frames/bkeytest_frame_ 24 animation.mp4
```

`RT_THREADS` changes the image as well as the speed: each band seeds its RNG
from its thread index. Compare hashes only at the same thread count.

**Logging**, via the environment rather than a rebuild — see `REFERENCE.md` for
the full grammar:

```bash
RT_LOG=off            ./build/raytracer assets/scenes/simple.lua
RT_LOG=debug          ./build/raytracer assets/scenes/simple.lua
RT_LOG=off,geom:debug ./build/raytracer assets/scenes/macho-cows.lua
RT_LOG_FILE=run.log   ./build/raytracer assets/scenes/simple.lua
```

**Tests and style:**

```bash
ctest --test-dir build -j 8 --output-on-failure    # 3.5 s; 15.8 s serially
scripts/check_style.sh                    # report
scripts/check_style.sh --fix              # rewrite the formatting
cmake --build build --target style        # the same check via CMake
```

Style is the Google C++ Style Guide: `.clang-format` holds the formatting
rules and `.clang-tidy` the naming ones. A pre-commit hook
(`git config core.hooksPath .githooks`) catches formatting on staged files, and
`.github/workflows/style.yml` catches both on every push. See the Style section
of `REFERENCE.md` for the two documented `NOLINT` exceptions.

**Conventions.** Sources are listed explicitly in `CMakeLists.txt` rather than
globbed, so a new file must be added there — it will fail to link rather than
silently not build. Source files are `lower_case.cc` / `lower_case.h`, and
includes are written relative to `src/` — `#include "geometry/mesh.h"`.

---

## 2. The renderer today

A **path tracer** — iterative throughput walk, Russian roulette, crude next
event estimation from point lights:

- **Materials** behind one `Eval` / `Pdf` / `Sample` interface: Lambertian,
  normalised Blinn-Phong, mirror, rough metal, dielectric (exact Fresnel, TIR,
  η² scaling)
- **Lighting:** lat-long environment map, point lights with shadow rays
- **Camera:** thin lens with shaped apertures (disk, hexagon, star, heart,
  crown)
- **Geometry:** spheres, boxes, OBJ triangle meshes, in a hierarchical scene
  graph — rays into local space, normals by inverse-transpose
- **Acceleration:** a per-mesh BVH, median or SAH split, recursive or iterative
  traversal, all selectable at run time
- **Output:** progressive accumulation in linear colour, tone-mapped once at
  write-out; Lua scenes; CSV-driven keyframe animation
- **Threads:** one per hardware thread (20 on this 14-core machine), static
  scanline bands
- Standalone build: no OpenGL, dependencies vendored

**Fixed along the way, before the staircase:** the 372× background-by-value
copy (28,316 ms → 76 ms, byte-identical); per-ray heap allocation in
`Sphere`/`Cube`/`NonhierBox`; per-thread `std::mt19937` replacing shared
`rand()`; the DoF/AA double-count that the `.1 *` fudge factor was hiding.

The three stubs step 8 inherited — `AABB::Hit()` returning `true`,
`BVH::Build()` leaving `built_` false, `BVH::Traverse()` never called — are all
real code since 5 October.

---

## 3. Step 0 — unblock

Not part of the staircase proper, but step 1 changes the render loop and step 2
changes write-out, so these were fixed while that code was already open.

| Issue | Symptom | Fix |
|---|---|---|
| Resolution capped | Segfault above 920×891 | Normalised, cover-scaled, clamped background lookup; renders at 2048×2048 |
| Built only under GCC | MSVC rejected `and`/`or` tokens and `polyroots.cc`'s `cbrt` | Both fixed; warning-free under GCC and MSVC |
| Double transform | Children of a `GeometryNode` transformed twice | Shared `ToLocal`/`ToWorld`, `HitChildren()`, regression test |

- [x] **Resolution is capped at 920×891 and segfaults above it.** The
      background centre-crop in `RayTraceRgb` indexed outside the decoded PNG
      when the render was larger than the texture:
      `offsetWidthIdx = bgWidthMid - cropWidthMid` went negative, unchecked.
      Verified: 512×512 fine, 920×920 and 1024×1024 both segfaulted. The
      background was about to be replaced by an environment light in step 3
      anyway.
- [x] **Portability: the code only built under GCC.** MSVC failed on the
      `and` / `or` alternative tokens and on `polyroots.cc` redefining `cbrt`
      (MSVC declares it dllimport, so redefinition is a hard error). Visual
      Studio's sampling profiler is therefore available for step 8's writeup.
      *The two builds do NOT render identically.* `Rng::Next` was made
      portable afterwards, taking the disagreement on `simple.lua` from 8.07%
      of pixels to 0.154%, but not to zero — see `REFERENCE.md`. Baseline and
      compare within one toolchain.
      *Correction:* `uint` was not an MSVC blocker as first diagnosed — it was
      a project typedef in `image.h`, not a MinGW type. It has been removed
      anyway, since a project-wide `uint` collides with the POSIX one.
- [x] **Latent: children of a `GeometryNode` are transformed twice.**
      `GeometryNode::IsHit` built `local_ray`, then handed it to
      `SceneNode::IsHit`, which applied the same inverse again — and both
      restored on the way out. `SceneNode::IsHit` also overwrote the hit
      material with the geometry node's own, clobbering a child's. No scene
      nested under a geometry node, so nothing was visibly wrong yet. The
      transform is now applied once via shared `ToLocal`/`ToWorld`, child
      traversal is factored into `HitChildren()`, and a regression test is in
      `tests/`.

---

## 4. The staircase

> **Note on shape.** Step 3 is the pivot. Steps 1–2 restructure the renderer
> you have. From step 3 onward you are building a **path tracer** —
> `Sample`/`Eval`/`Pdf`, the furnace test, next event estimation and MIS are
> all path-tracing machinery, and `PhongMaterial` (ad-hoc `kd`/`ks`/shininess,
> not energy-conserving, no pdf) gets replaced rather than extended. Steps 4,
> 10 and 11 all depend on that interface existing.
>
> *(Done. It was substitutive, exactly as warned: `PhongMaterial` is gone, and
> so are the recursive `glm::mix` reflection and the ad-hoc ambient term.)*

> **Note on order.** The steps are in **working order, not numeric order**,
> and keep their original numbers: `ENGINEERING.md`, `WALKTHROUGH.md` and
> cross-references in this file all name them, and renumbering would silently
> break every one.
>
> **In scope for 10 October 2026:** steps 4, 5, 8, 9, in that order, with step
> 12 as a stretch goal after them. (Moved from 30 September; see
> `ENGINEERING.md`.) Steps 6, 7, 10 and 11 are deferred to §5. The "do not
> start N+1 until N passes" rule is broken here on purpose — it still holds
> within the in-scope sequence.

---

### Step 1 — Pixel jitter + accumulation buffer

| | |
|---|---|
| **Goal** | Jitter the ray inside the pixel footprint; keep a running radiance sum and a sample count; divide at write-out. "Accumulate forever, snapshot anytime." |
| **Done when** | Edges are smooth, and an image can be dumped at any sample count without re-rendering. **Met.** |
| **Result** | RMS against the final image falls monotonically 0.820 → 0.083 (4 → 60 spp) |

**What was built.** `src/render/framebuffer.{h,cc}` holds a per-pixel `dvec3`
sum plus a sample count; `Resolve()` divides into an `Image` and is `const`, so
a snapshot never disturbs the accumulation. A pass adds one jittered sample to
every pixel.

- `samples` on the `gr.render` table — total samples per pixel. Was
  `gr.set_samples(n)`, retired once every render setting moved onto that table.
- `gr.set_snapshot_interval(n)` — writes `<out>_NNNNspp.png` as it goes.

**Measured, to prove snapshots are the same image converging:** a single 64
spp render emitted 16 images, and RMS against the final result fell
monotonically 0.820 → 0.083 (4 → 60 spp). The sphere silhouette goes from hard
stair-steps at 1 spp to a smooth gradient at 64.

**Left for later:**

- Jitter is **uniform**, not stratified. Stratified samples converge faster for
  the same count (§7 backlog).
- The sample count is global, not per pixel. Adaptive sampling will need
  per-pixel counts.
- Thread bands own disjoint rows, so `Add()` needs no atomics. Step 6 keeps
  tiles disjoint too, but recheck that assumption then.

Thread count now comes from `hardware_concurrency()` rather than a hardcoded
16, which is why timings improved slightly on a 14-core, 20-thread machine.

---

### Step 2 — Linear color pipeline

| | |
|---|---|
| **Goal** | Radiance stays linear internally; sRGB only at write-out; tone mapping a separate, swappable stage |
| **Done when** | Tone mapping can be disabled for a raw linear dump, and a 0.5 albedo surface under a 1.0 light reads 0.5 in linear, not 0.73. **Met.** |
| **Result** | `Image::SavePng` is the only place bytes are made |

**What was built.** `core/tone_map.{h,cc}` holds both stages: `Apply()` (`None`
/ `Reinhard` / `ReinhardExtended`; `ACES` is a stub) and `EncodeSrgb` /
`DecodeSrgb`. `Framebuffer::resolve` stays linear. The background PNG is
`DecodeSrgb`'d on input, replacing the old flat `0.3` scale.

- `gr.set_tonemap{ operator=, exposure=, white_point=, srgb= }` — all optional;
  `srgb = false` is the raw linear dump.
- `tests/scenes/tonemap_probe.lua` checks both dumps stay consistent.

**Issue found:** at the time the probe read ~0.375, not 0.5 — the always-on
reflection `glm::mix` blended in 25% black. Step 3's BSDF removed it.

---

### Step 3 — BSDF interface + furnace test

| | |
|---|---|
| **Goal** | Define `Sample(wo, rng) -> {wi, throughput, pdf}`, `Eval(wo, wi)`, `Pdf(wo, wi)` before there are many materials; port diffuse; build the furnace test (uniform environment of radiance 1, albedo-1 diffuse sphere) |
| **Done when** | The sphere is invisible against the background. Darker means losing energy; brighter means double-counting. **Met.** |
| **Result** | Energy conservation proven at two levels: integrated BSDF tests and rendered furnace scenes |

**What was built.** `Material` is now a pure BSDF interface (`Eval` / `Pdf` /
`Sample`), implemented by `LambertianMaterial` and `BlinnPhongMaterial` — the
latter a normalised `(n+2)/8π` lobe with luminance-weighted two-lobe sampling,
so `gr.material` is energy-conserving for `kd + ks ≤ 1`. `RayTraceRgb` is an
iterative throughput walk with Russian roulette; the recursive `glm::mix`
reflection, the ad-hoc ambient term and the whole Blinn-Phong inline block are
gone.

Both materials are reachable from Lua as `gr.lambertian{ kd = ... }` and
`gr.blinn_phong{ kd = ..., ks = ..., shininess = ... }`, with `gr.material`
kept as a deprecated positional alias so existing scenes load unchanged.

**How it is verified, at two levels:**

- **`tests/bsdf_test.cc`** (its own CMake target) integrates the BSDFs
  directly, with tolerances at 4 standard errors computed by Welford from the
  run itself. White-furnace ρ = 1, `E[cosθ] = 2/3` for the cosine sampler, and
  energy conservation across five `kd`/`ks`/exponent cases at three angles of
  incidence. Two of the checks tie `Sample()` to `Pdf()` without the
  cancellation trap — the obvious `f·cos/p` estimator is degenerate for a
  Lambertian and returns the albedo even if the sampler is broken. Deleting the
  half-vector Jacobian fails the sampler checks, so the tests bite.
- **`tests/scenes/furnace.lua`** is the criterion: uniform 255 at radiance 1,
  uniform 128 at radiance 0.5. The second render exists because 255 clips,
  which would hide a too-bright result. `FURNACE_MATERIAL` picks the material,
  so the same scene is the criterion for the diffuse, the mirror and the metal.

**Issues found:**

| Issue | Why it matters | Resolution |
|---|---|---|
| 11 of 12 scenes had `kd + ks > 1` (`simple.lua` worst, 1.7 in green) | Harmless under Whitted shading; compounds every bounce under a path tracer | Scene load now warns per channel |
| Diffuse divided by π | Diffuse went from `kd·N·L` to `(kd/π)·N·L`, so every scene's lights were scaled by π; specular gained `(n+2)/8π` instead, so highlights are stronger than before | No single factor fixes both — that is what an energy-conserving lobe does |
| Blinn-Phong dim at grazing | ρ = 0.835 at 0° but 0.056 at 80° (`ks = 0.9, n = 50`); 15% of samples at 80° fall below the horizon | Expected: no Fresnel, no multiple scattering. MIS (step 11) would recover the lost samples |

**Carried forward:** the background is now a lat-long environment map sampled
by ray direction (`gr.set_background`), so it lights the scene rather than just
backing it. An empty path means `ambient` is a uniform environment. The old
screen-space lookup could not work for bounce rays, which have no pixel.

---

### Step 4 — Refraction and reflection

| | |
|---|---|
| **Goal** | Dielectrics (Snell, TIR, Fresnel), smooth metal, rough metal, all through step 3's interface |
| **Done when** | A glass sphere shows caustics and correct TIR at grazing angles, and an albedo-1 dielectric is invisible in the furnace |
| **Status** | **Closed 13 September 2026.** Every rung ticked; caustic measured |
| **Result** | Caustic core **1.98×** the open floor, **2.43×** the shadow annulus |
| **The lesson** | The furnace scores *energy*, never *direction*. Most of this step's bugs were direction bugs, so most of its tests had to be direct ones |

**Where you stand:** the dielectric reflects and refracts, splits between the
two by Fresnel, reflects totally past the critical angle, and scales the
transmitted weight by the relative η². Adding a material is a new `Material`
subclass plus a `gr.*` constructor and one row in `grlib_functions`;
`push_material` is the shared tail and `set_material` never learns the
concrete type.

`Refract()` satisfies Snell at every angle it is asked about, `Sample()` stops
asking past the critical angle, and the branch it takes is drawn with Fresnel's
probability. All three are held by `tests/bsdf_test.cc`, suite
`bsdf/dielectric`: fifteen crossings of glass in air and air in water, entering
and exiting, on both sides of each critical angle, plus a measured reflected
fraction against independently computed reflectance.

#### Issues and fixes at a glance

| Rung | What went wrong | Why | Fix |
|---|---|---|---|
| Mirror | Mirror sphere one code darker than its environment | `brdf·cos/pdf` divides out a cosine and multiplies it back; not exact in float | Specular hits multiply `brdf` straight into throughput |
| Rough metal | Planned as a density that passes the furnace — both halves false | No closed-form density; fuzz tips rays below the surface, where they are absorbed | Stays a delta; criterion is "loses energy, never gains any" |
| Transmit only | Transmitted rays self-hit their own surface | Bounce origin always nudged outward along the normal | Offset follows the scattered direction, and is scale-relative |
| Transmit only | Furnace passes with TIR demonstrably broken | Throughput 1 in a uniform environment: blind to direction | A direct `Refract()` test |
| `Refract()` test | Planned "unit length" tell useless | `Sample()` normalises, so a wrong-length vector arrives as a wrong *direction* | Length asserted on the helper; hemisphere and Snell through the material |
| `Refract()` test | TIR condition tested `index_`, not `index_ratio` | Reciprocals entering, equal exiting: half right | Fixed; 10 assertions failed on exactly 4 crossings |
| TIR | Past the critical angle, a 1.15–1.48-length tangent was normalised into a skimming ray | `Refract()` clamped instead of refusing | `Sample()` decides before calling |
| TIR | The measurement swept only *exiting* glass | One direction of a two-direction interface | `glass_spheres_TIR.lua`: an air bubble in water |
| Fresnel | Schlick wrong at both ends: 92% reflection off a non-interface at 89°, flat 0.04 then a jump at the critical angle | An approximation used outside its range | Exact dielectric Fresnel equations |
| Fresnel | Furnace failed **dark** (`lo = 233` vs 254) | Schlick's grazing 0.92 kept rays rattling inside until `kMaxDepth` | Exact Fresnel |
| Fresnel | A TIR test passed by luck | Single seeded draw on a now-probabilistic branch | Statistical cases over 200k draws |
| η² | Sign predicted backwards | Renderer transports importance, not radiance (Veach) | Entering darkens by 1/η², leaving brightens by η² |
| η² | Furnace blind to the factor; sign-flipped build **byte-identical** to none | Roulette's `q = min(0.95, max(throughput))` clamps both | Sign scored in `bsdf_test.cc` only; furnace asserts the frame mean |
| η² | A delta-contract test passed by luck | `brdf == 1` on one seeded draw | Uses the two crossings that weigh 1 either way |
| Caustic | Point lights and uniform skies cannot make one | NEE occlusion discards the caustic path; uniform in, uniform out | A small sun in the environment map |
| Caustic | Standing advice to raise `kRrStartDepth` was wrong | Roulette kills *low* throughput; caustic paths are bright | Measured: 1.98× and 12.2% noise at depth 3 and 8 alike |

#### Two ideas the rungs rest on

**A dielectric reflects AND refracts.** Not one or the other. At every
interface Fresnel splits the energy: a fraction `R(θ, η)` reflects, `1 - R`
transmits, and `R` climbs toward 1 at grazing incidence until total internal
reflection makes it exactly 1. Glass that only refracts has no highlights and
no bright rim, and looks wrong on sight.

The transmitted half breaks an assumption both diffuse materials share:
`LambertianMaterial::eval` and `BlinnPhongMaterial::eval` return black when
`dot(normal, out) <= 0`, treating the far side of the surface as *no
contribution*. For a dielectric that direction is legitimate, so the guard has
to compare the sidedness of `view_dir` and `out` rather than assume they match.

**Specular lobes are delta distributions**, and no `float` pdf can say
"infinite here, zero everywhere else". The convention: `IsSpecular()` marks the
material, `Eval()` and `Pdf()` return 0, and `Sample()` returns `pdf = 1` with
the whole weight in `brdf`, which the renderer applies unmodified. Returning 0
is correct rather than a cop-out — next event estimation can never land on a
delta lobe, for the same zero-measure reason BSDF sampling can never hit a
point light.

#### The rungs, in detail

Each rung builds, renders, and has its own pass/fail signal — the finished
dielectric was not written in one go.

- [x] **Perfect mirror.** No refraction at all: `Sample()` reflects `view_dir`
      about `normal` and returns `pdf = 1` with `brdf = albedo`. This rung
      exists to force the delta-pdf plumbing while nothing else is moving.
      *Signal:* an albedo-1 mirror sphere in the uniform furnace must be
      **invisible**, since it reflects radiance 1 from every direction.
      **— met.**

      `IsSpecular()` joins the `Material` interface, defaulting to `false`,
      and `renderer.cc` branches on it to multiply `brdf` straight into the
      throughput. The general `brdf * cos / pdf` estimator would divide out a
      cosine and multiply the same one back; that round trip is not exact in
      float, and the drift left the mirror one code darker than its
      environment. Lua: `gr.mirror{ albedo = {...} }`.

      `MeasureSampler()` assumes a density, so it now skips a specular
      material rather than false-failing on it, and `CheckDeltaContract()`
      asserts the real invariant: `pdf == 1` and `brdf == albedo` exactly, on
      every draw. `tests/scenes/furnace.lua` with `FURNACE_MATERIAL=mirror`
      renders at radiance 1 and 0.5, both perfectly uniform
      (`min == max == 255` and `128`).

- [x] **Fuzzy reflection (rough metal).** A specular lobe centred on the
      mirror direction, widened by a fuzz parameter. Placed right after the
      mirror, because it needs none of the dielectric machinery: no Fresnel
      split, no Snell transmission, no TIR — just the reflection half,
      blurred. **— met.**

      `MetalMaterial` draws `normalize(reflect(in, normal) + fuzz * u)` for a
      uniformly random unit `u`. Displacing a unit vector by `fuzz` and
      renormalising sweeps a cone of half-angle `asin(fuzz)` — the tangent
      from the origin to the offset ball bounds the lean — which is what makes
      `fuzz = 1` the widest lobe and why the constructor clamps to `[0, 1]`.
      Reachable from Lua as `gr.metal{ albedo = {...}, fuzz = f }`.

      **Planned wrong in two ways, both worth keeping.** This rung was written
      as *a real density, not a delta, so `MeasureSampler()` applies to it
      unmodified*. Both halves are false: there is no closed-form density to
      return, so the lobe stays a delta and `CheckDeltaContract()` is what
      applies. A density would need a real microfacet distribution, which is a
      different rung entirely.

      The predicted signal was *still passes the furnace at every fuzz value*.
      It does not, and should not. A wide enough perturbation tips the
      scattered direction into the surface; that ray is absorbed, reported as
      `pdf = 0`, and ended by the renderer's `pdf <= 0` guard. A rough metal
      therefore loses real energy and is **darker at its silhouette**, so the
      criterion is *loses energy, never gains any* rather than invisibility.
      Only `fuzz = 0` is invisible in the furnace, and that is the mirror.

- [x] **Make the skeleton reachable, and transmit only.** No Fresnel yet. Fix
      the `Refract()` sign, return `brdf = 1` with `pdf = 1`, bind
      `gr.dielectric{ ior = ... }`, and add a `dielectric` entry to
      `furnace.lua`. **Make the surface offset sign-aware** — `renderer.cc`
      nudged the bounce origin to `hit_point + N * kEpsilon`, always outward,
      which put a transmitted ray on the wrong side of its own surface and let
      it immediately self-hit. It must follow the scattered direction, not the
      normal. That is the only renderer-side change: the `dot(normal, out) <= 0`
      guard lives in the two diffuse `Eval`s, which a delta dielectric never
      calls, and the `pdf <= 0` break passes a `pdf` of 1 through untouched.
      **— met.** `assets/scenes/glass_spheres.lua` reaches it, and the offset
      is now scale-relative as well as sign-aware, for a reason that had
      nothing to do with dielectrics.

      **The predicted signal was met, and the reasoning behind it was still
      wrong.** It read as though `index = 1.0` were the weak case and a higher
      index would test more. It is not: at `ior = 1.5`, with TIR demonstrably
      broken, the furnace still renders 65536 of 65536 pixels at exactly 128.
      A dielectric's throughput is identically 1 and the environment is
      uniform, so *every* scattered direction returns the same radiance. The
      furnace is an energy test and it is structurally blind to direction
      errors for this material — at any index. Raising it would buy false
      comfort, which is why `furnace.lua` keeps 1.0 and says so.

      The consequence is a split, not a blanket failure. The furnace still
      scores *energy* errors on the rungs below, but not uniformly: a missing
      η² factor cancels over any path that both enters and leaves the glass,
      and the η² rung measured that it does not surface on the paths that die
      inside either. What the furnace can never score is a *direction* error,
      and TIR and Snell are exactly that. Those need a signal it cannot give.

- [x] **A direct test for `Refract()`.** First, because it is what scores the
      rungs after it. Sweep incidence angles entering and exiting, and assert
      unit length, Snell below the critical angle, and the correct hemisphere
      above it. A dozen lines, no renderer, and the only check that failed at
      the time. *Signal:* it fails on the old TIR handling before it passes on
      the fixed one — the same standard the half-vector Jacobian was held to
      in step 3. **— met**, and it found more than it was written for.

      **The predicted failure and the actual failure were different
      failures.** The plan named unit length as the tell, which is true of
      `Refract()` and useless through `Sample()` — `Sample()` calls
      `glm::normalize` on the result, so the long tangent vector reaches the
      caller as a perfectly unit direction pointing the wrong way. Length is
      now asserted on the helper directly, for the crossings where Snell has a
      solution; through the material, the assertions that bite are hemisphere
      and Snell's angle. A test written one layer too high would have passed
      on the broken code.

      It also caught a **second error nobody had looked for**: the TIR
      condition tested `index_` where it needed `index_ratio`. Those two are
      reciprocals on the way in and equal on the way out, so the line read as
      correct while being half correct — right in every exiting case, wrong in
      every entering one. Ten assertions failed, on exactly four crossings,
      and no others.

- [x] **Total internal reflection.** When `sin²θt > 1`, reflect entirely.
      Small in code, but a **correction, not an addition**: `Refract()` used to
      clamp the parallel term to zero past the critical angle and return the
      perpendicular component alone, a tangent vector of length 1.15 to 1.48
      that `Sample()` normalised into a ray skimming along the surface.
      *Signal:* the direct test above, not the furnace — which passes either
      way. **— met.** `Sample()` now decides before calling, and `Refract()` is
      documented as having no answer above the critical angle rather than
      being made to invent one.

      **The measurement this rung was written from was one-sided.** The table
      swept glass at η = 1.5 *exiting*, which is the direction the condition
      happened to get right, so it established that TIR was mishandled without
      touching the case that was mishandled worse. The entering direction was
      never sampled, and that is where a ray was being mirrored at any angle
      past 41.81° — across 55% of a sphere's projected disc, since
      `sinθ = r/R`. Sweeping one direction of a two-direction interface is the
      general form of the mistake.

      `assets/scenes/glass_spheres_TIR.lua` uses `ior = 1/1.33`, an air bubble
      in water, where the critical angle is reached going **in** rather than
      out — the mirror image of the glass case, and the half the old table
      missed. It now renders as a crescent with a dark rim instead of a washed
      out smear.

- [x] **Fresnel split.** Choose between the two branches with probability
      `R(θ)` rather than always transmitting. Both branches already existed
      and were tested, so this rung is the choice and nothing else — **do not
      absorb the remainder.** An earlier draft had the non-reflected fraction
      go to black, which made sense when transmission did not exist; doing it
      now would delete working physics to stage a picture that is deliberately
      wrong. It is also what finally uses `Sample()`'s `rng`, unused since the
      material was written. **— met**, with one instruction overturned.

      *Signal planned:* **not the furnace.** Both branches carry throughput 1,
      so any mixture of them returns radiance 1 in a uniform environment — the
      same blindness the transmit-only rung ran into. Count draws instead: the
      reflected fraction over many samples must match `R(θ)`, near 0.04 at
      normal incidence for `ior = 1.5` and climbing to 1 at grazing.

      **Schlick was the wrong primitive, and this rung named it in the
      title.** It is accurate for a typical interface at moderate angles and
      wrong at both ends of the range this material actually spans. At
      `index_ratio` 1 there is no interface and R must be 0 everywhere;
      Schlick returns `r0 + (1 - r0)(1 - cosθ)⁵`, which climbs to 1 at grazing
      whatever `r0` is, so it reflects 92% of rays at 89° off a surface that
      is not there. Approaching the critical angle from inside it stays flat
      near 0.04 and then hands over to a TIR branch at 1.0.

      **Schlick takes the cosine on the thinner side, and that is not always
      the incident one.** It is derived for a ray entering the denser medium;
      fed the incident cosine on the way *out* of glass it returns ≈0.04 right
      up to the critical angle and then hands over to a TIR branch that
      reflects everything — a step from 0.04 to 1 with nothing in between,
      which renders as a hard ring. Evaluated on `cos θt` when
      `index_ratio > 1` it climbs to exactly 1 as `θt → 90°`, meeting the TIR
      branch continuously because that limit *is* the critical angle. The
      ring is the signal: if the rim has an edge, the wrong cosine went in.
      Leaving glass at η = 1.5, critical angle 41.81°:

          thetaI   Schlick(cos_i)   Schlick(cos_t)   exact Fresnel
            0.0       0.0400           0.0400          0.0400
           35.0       0.0402           0.0672          0.0861
           40.0       0.0407           0.2456          0.2453
           41.8       0.0410           0.9075          0.8908

      The middle column is the one that meets the TIR branch. The left column
      is flat across the whole approach and then jumps.

      **The fix: the exact dielectric Fresnel equations.** They fix both ends
      and cost a `sqrt` and two divides on a lobe evaluated once per bounce.
      They also delete the `cos θt` correction this rung used to prescribe:
      the exact form takes the incident cosine and handles either side itself,
      so the special case stopped existing rather than getting implemented.
      Leaving `brdf` at 1 on both branches stays right — choosing with
      probability R and weighting R/R cancels exactly.

      **The furnace did catch it, and the reason is worth keeping.** This rung
      predicted it could not, and the furnace still cannot score the *ratio*.
      What it caught was longer paths — Schlick reflecting ~0.92 at grazing
      sends rays rattling inside the sphere until they exhaust `kMaxDepth` and
      return black. The furnace failed **dark**, `lo = 233` against an
      expected 254. Roulette never got near them, because throughput never
      drops. A test can be blind to a quantity and still see what that
      quantity does to path length.

      **One test had been passing by luck.** The TIR case asserted
      `reflected == expected` from a single `DrawOnce` at a fixed seed. Once
      the branch became probabilistic that assertion was a coin flip, and it
      passed only because `Rng(85)`'s first draw happened to exceed R in all
      fifteen crossings. Three cases in the suite are now statistical, over
      200k draws each, and a sixth measures the split itself against
      reflectances computed outside the renderer — comparing `Reflectance()`
      to itself would agree however wrong it was. That case rejects Schlick on
      five rows, worst 0.387.

- [x] **η² radiance scaling across the interface.** Radiance is not invariant
      through refraction; it scales by the relative η². `brdf` was 1 on both
      branches, which is right for reflection and wrong for transmission.
      **— met.** The transmitted branch now carries `index_ratio²`, and
      `tests/bsdf_test.cc` sweeps it over every crossing in `kCrossings`.

      **The exponent came from the renderer, not from the physics.** "Radiance
      scales by η²" is about light travelling into the denser medium.
      `RayTraceRgb` starts at the camera and carries a throughput forward along
      the reverse path, so what it transports is importance, which scales as
      radiance's reciprocal — Veach's non-symmetry of refraction. Entering
      therefore *darkens* by 1/η² and leaving brightens by η², which is the
      opposite of what this rung predicted: the prediction was the physics
      statement applied to the wrong quantity.

      **The furnace prediction was wrong, and the reason is sharper than a
      near miss.** This rung expected the paths that die inside the glass to
      leave the entering factor uncancelled and come back bright. They cannot:
      a path that dies contributes nothing at all, so there is nothing for an
      uncancelled factor to scale. Every path that *does* reach the
      environment has crossed out as many times as it crossed in, so the mean
      is unchanged and the furnace is blind to this rung at any index — not
      narrowly, but exactly.

      **It is blind to the sign for a second and stronger reason.** With the
      sign flipped, the glass furnace is **byte-identical** to no η² factor at
      all. Russian roulette uses `q = min(0.95, max(throughput))`, so a
      throughput of 2.25 inside the glass and a throughput of 1 both clamp to
      0.95 — same draws, same stream, same image. The furnace never sees the
      factor, rather than seeing it and cancelling it. At `ior = 1.5`, 64 spp,
      radiance 0.5:

          variant                     full (255)   half (128)
            no η² (`brdf = 1`)        232–255      116–131
            η², sign flipped           232–255      116–131
            η², as shipped             216–255      108–141

      The third row is the only one that moves, and it moves in **both**
      directions: 1/η² drops the throughput below roulette's clamp, so paths
      inside the glass die more often and the survivors are boosted to
      compensate. Unbiased, noisier. So the invisibility criterion every other
      material meets would fail here on variance alone, and would be measuring
      roulette rather than the BSDF. `furnace.lua` gains a `dielectric_glass`
      entry and `render_test.cc` asserts its frame **mean** instead, which
      still catches a gross leak — applying the entering factor without the
      leaving one measures 98.5 against 128 — while claiming nothing about the
      sign. The sign is scored in `bsdf_test.cc` and only there.

      `assets/scenes/glass_spheres.lua` says the same thing away from the
      furnace: against the same scene rendered without the factor, the frame
      mean moves from 128.522 to 128.526 while single pixels move by as much
      as 65 bytes. Four thousandths of a byte of bias, and visible extra grain
      on the glass. Scored on a picture, this rung would have been called a
      no-op.

      **Square the index, not the ratio.** `1/(η·η)` times `η·η` is exactly 1
      in float at 1.5 and at 1.05, where `(1/η)·(1/η)` times `η·η` is not; it
      is never worse. It is also not universal — diamond at 2.417 lands one
      ulp short either way — so the round-trip test records that rather than
      claiming exactness for every index. This is the same concern that left
      the mirror one code darker than its environment in the first rung.

      **A test was passing by luck again, in the same way.** The
      delta-contract case asserted `brdf == 1` on a single seeded draw
      entering glass. Once transmission stopped weighing 1, that assertion was
      correct only when the seed happened to reflect. It now uses the two
      crossings that weigh 1 whichever branch they draw: index 1, which is not
      an interface, and a crossing past the critical angle, where only the
      reflected branch exists.

#### The caustic — the last criterion

**Measured, so "a caustic" is a number rather than an impression.**
`assets/scenes/caustic.lua`: a glass ball of radius 0.5 at `ior = 1.5`,
floating 0.3 above a diffuse floor, lit by a 4°-radius sun in a lat-long
environment map. A ball lens focuses at `nR / (2(n-1))` = 0.75 from its
centre, which lands the focus at `y = -0.45`, just above the floor at `-0.5` —
that is what makes the spot a point instead of a smear. Across the focal row at
512 spp:

    open floor        74.7
    shadow annulus    60.9     82% of floor
    caustic core     147.7    1.98x floor, 2.43x annulus

Dark ring, bright core. `docs/images/step4-caustic.png`.

**Getting a caustic at all took choosing the light source, and two of the
three options cannot carry one.** This is the part worth writing up:

- **A point light cannot.** The crude NEE loop in `RayTraceRgb` casts a shadow
  ray from the hit point to the light and skips the light if *anything* is in
  the way. A dielectric is "anything". So a point light behind glass produces a
  **shadow**, never a bright spot — the caustic path is precisely the one the
  occlusion test discards. Recovering it needs light-path methods (photon
  mapping, bidirectional), which are not merely deferred but absent from the
  plan entirely.
- **A uniform environment cannot**, for the furnace's exact reason: refraction
  redistributes uniform radiance into uniform radiance, so the focus carries no
  more energy than the floor beside it. `glass_spheres.lua` has a flat sky and
  shows no caustic, and that is correct behaviour, not a missing feature.
- **A small bright region in an environment map can**, because BSDF sampling
  can reach it: floor → diffuse bounce → glass → two refractions → sun.

**The standing warning about Russian roulette was wrong.** This section used to
say roulette was most likely to kill caustic paths and to raise `kRrStartDepth`
before doubting the BSDF. Rebuilt with `kRrStartDepth` at 8 instead of 3, the
same render gives core/floor of **1.98× either way**, floor noise of **12.2%
either way**, and the same frame time to within noise. Roulette kills paths by
*low throughput* — `q = min(0.95, max(throughput))` — and a caustic path is a
bright one: floor albedo 0.75, dielectric branches weighing about 1, so `q`
never falls near zero. The advice named the wrong mechanism.

**What the remaining noise is:** the sun subtends 0.0153 sr, so a
cosine-weighted diffuse bounce finds it about **0.49%** of the time, and BSDF
sampling is the only way to find it — next event estimation is step 10,
deferred. That is why 512 spp still grains. It is a sampling-strategy limit,
not a BSDF error, and step 10 is what fixes it.

#### Left open

- `assets/scenes/final_animation.lua:33` and `:37` call `gr.material` with
  **six** arguments — the trailing `0.0, 0.0, 1.0` and `0.4, 0.0, 1.0` look
  like reflectivity, transparency and IOR. Lua silently discards them and
  always has, so those two lines are lies in the scene file. Delete them, or
  make them real now that the dielectric constructor exists. The
  table-argument constructors (`gr.blinn_phong{...}`) would have rejected an
  unknown field; the deprecated positional `gr.material` still will not.

---

### Step 5 — Thin-lens camera

| | |
|---|---|
| **Goal** | Sample a point on the aperture, aim through the focal plane; expose the lens through the scene language |
| **Done when** | You can rack focus between a near and a far sphere |
| **Status** | **Done 22 September 2026, measured** |
| **Result** | The focused sphere holds **0.99×** and **1.01×** of its pinhole sharpness; the others fall to 0.32–0.60× |
| **The lesson** | The lens was the first code to use the camera basis as *directions*, and it exposed two old bugs the old code had cancelled out |

#### Issues and fixes at a glance

| Issue | Symptom | Cause | Fix and evidence |
|---|---|---|---|
| Camera basis mirrored | Invisible — the image still came out upright | `u` was screen left, `v` screen down; the old stepping cancelled both | `u = cross(w, up)`, `v = cross(u, w)`, signs written at the point of use |
| Half-pixel offset | Frame half a pixel off; sample window overhung the top-left | Stepped by `x`, `y` with no `+0.5`: sampled pixel corners | `+0.5`; asymmetry 0.592 → 0.054 at 6400 spp |
| `h / 2` in `size_t` | Odd heights truncated (225 → 112) | Integer division | Computed in float |
| Curved focal surface (prevented) | Off-axis spheres blur at the frame edges | Dividing by `length(pin_dir)`, not the axial component | Divide by `dot(pin_dir, w)`; edge ratio 0.68 shows what it saves |
| Shaped aperture would break every BSDF | Tests stay green while `Pdf`/`Sample` disagree | `SampleUnitDisk` serves both the lens and Malley's method | The lens got its own sampler (`aperture.h`), 25 Sep |

#### The rack focus — the criterion, measured

`assets/scenes/thin_lens.lua` renders three spheres at three depths from one
eye — through a pinhole, focused near, focused far — at a matched 256
rays/pixel so blur is the only variable. Mean gradient magnitude in a ±4 px
ring on each sphere's silhouette:

    shot                near ball   mid ball   far ball
    pinhole                  8.70       8.08      17.13
    near  (focus 2.05)       8.59       4.88       6.54
    far   (focus 7.01)       2.78       3.76      17.22

`docs/images/step5-rack-focus-pinhole.png`, `-near.png`, `-far.png`.

**Why compare with the pinhole:** an in-focus edge is exactly as sharp as a
pinhole edge — the aperture can only cost sharpness away from the focal plane.
That is the invariant to check first if this ever regresses.

#### Issues in detail

**The camera basis was mirrored and the pixel grid was off by half a pixel.**
Found while writing the lens, because the lens is the first code to consume
`u_vec`/`v_vec` as directions rather than as step vectors.
`u = cross(up, view)` is screen *left* and `v = cross(u, w)` is screen *down*;
`RenderBand` cancelled both by stepping `(w - x)` and `+y` from a top-right
corner, so the image came out upright and the mirroring stayed invisible. The
basis is now `u = cross(w, up)` (right) and `v = cross(u, w)` (up), with the
orientation signs written at the point of use: `+(x + 0.5)*u` and
`-(y + 0.5)*v` from a top-left corner.

The half-pixel was the real bug. Stepping by `x` and `y` with no `+0.5`
samples pixel *corners*, so the frame sat half a pixel off in both axes and
the sample window overhung the top-left edge. **Measured on a scene built
mirror-symmetric about both screen axes**, where correct registration must
render symmetrically:

    spp     L-R asymmetry      T-B asymmetry
    400     0.622  before      0.623  before
    400     0.219  after       0.217  after
    6400    0.592  before      0.593  before
    6400    0.054  after       0.053  after

**Why two sample counts:** they separate bias from noise. 16× the samples
barely moves the old numbers — a systematic bias does not average away — while
the new ones fall by almost exactly 4× (= 1/sqrt(16)), which is Monte Carlo
noise converging to zero. Also fixed in the same pass: `d_float` computed
`h / 2` in `size_t`, truncating for odd heights (225 to 112).

**The focal plane is a plane because of a dot product.** `focus_t` divides by
`dot(pin_dir, w_vec)`, the axial component, not by `length(pin_dir)`. Dividing
by the length places each focal point a fixed distance along *its own ray*, so
the locus is a sphere around the eye rather than a plane.

**Measured, because a centred rack focus cannot see it:** seven identical
spheres, all at the same axial distance, spread across the frame at 800x450
and 40° fov, aperture 0.70, focused on exactly their distance. A flat focal
surface puts every one of them in focus; a curved one cannot. Silhouette
sharpness, and the ratio between the two builds:

    sphere      x=-5.4  x=-3.6  x=-1.8  x=0.0  x=+1.8  x=+3.6  x=+5.4
    dot (flat)   19.24   25.86   27.10  26.90   27.12   25.84   19.25
    length       13.16   23.35   26.93  26.88   26.89   23.31   13.29
    ratio         0.68    0.90    0.99   1.00    0.99    0.90    0.69

`docs/images/step5-focal-plane-flat.png` and `-curved.png`. The ratio row is
the statistic that matters — the absolute numbers fall off at the edges in
*both* builds, because an off-axis sphere projects to an ellipse and the
measurement ring is a circle. The centre is **1.00**: the two divisors agree
exactly on axis, which is why a rack focus on a centred subject cannot detect
this error at all. It only shows up off-axis, where nobody is looking.

The curved build was produced by editing the one divisor, rendering, and
reverting — the same technique as the half-pixel measurement, and worth
repeating whenever a bug is invisible in the shipped build.

**`SampleUnitDisk` had two consumers, and that was a trap.** Step 3 made it
the body of cosine-weighted hemisphere sampling as well, via Malley's method —
a uniform disk sample lifted to the hemisphere is cosine-distributed, which is
why one function served the aperture and the BSDF. So a *shaped* aperture
(hexagonal bokeh, a bladed iris) is correct for the lens and would silently
break every BSDF's `Pdf`/`Sample` agreement, because the pdf still assumes a
uniform disk. **Neither the furnace nor `pdf mass == frac above horizon` catches
it** — measured 22 Sep by swapping a hexagon into `SampleUnitDisk`. All 10
render tests passed, and the pdf-mass pair stayed at 1 because `Pdf()` never
changed. The moment checks catch it: mean cosine 0.7437 against 2/3, and the
`cos²` integral in both the Lambertian and Blinn-Phong suites. So does
`lambertian: two constructions of the cosine lobe agree`, whose reference draws
from `SampleUnitBall` and cannot move with the aperture.

*Fixed 25 Sep, in that order:* `src/render/aperture.h` carries the shaped
cut-outs and `ThinLensRay` draws from it, leaving `SampleUnitDisk` to Malley.
The disk case still forwards to `SampleUnitDisk`, and the disk frame of
`bokeh.lua` came out byte-identical across the split.

#### Design notes

**`samples` and `lens_samples` are one budget, not two.** `total_samples` is
their product and nothing downstream sees the factors, because `RenderBand` is
a single flat loop: each iteration jitters the pixel *and* draws one aperture
point, so every ray is both an AA sample and a lens sample. 16x16, 256x1 and
1x256 render **byte-identical**. That is the correct Monte Carlo structure —
256 independent samples of a 4-D space — but the API implies a decomposition
the renderer does not have. `lens_samples` is a cosmetic multiplier today;
stratifying both domains is what would make it real (§7 backlog).

Two sharp edges left in place: `Enabled()` gates on `samples > 0`, so
`lens_samples = 0` silently yields a pinhole rather than an error; and the lens
multiplier only applies when `aperture_radius > 0`, which is what lets
`thin_lens.lua` pass `lens_samples = 16` on its pinhole shot without tracing
4096 rays.

**The lens is an angle, not a radius.** `defocus_angle` is the full apex angle
of the cone from a point on the plane of focus back to the rim of the lens,
which is scale-free where a radius in world units is not: 0.25 means something
different in a scene measured in metres and one measured in hundreds. The cost
is that holding a *radius* fixed across a focus pull — which is what a real
lens does — needs a different angle at each focus distance.

**What the model assumes, and therefore cannot do.** A thin lens has area but
no thickness, no glass and no aberration:

- **The disk is sampled uniformly**, so the bokeh is a uniform disc. Real
  optics vignette — the aperture a corner pixel sees is a lens-shaped sliver,
  not a circle — and there is no `cos^4` falloff here either. Both would be
  additions to `ThinLensRay`, not corrections to it.
- **Nothing is chromatic.** One focal point serves all three channels, so there
  is no longitudinal or lateral colour fringing at any aperture.
- **The ray direction stays unnormalized, and its length now depends on the
  camera.** A pinhole ray has `|dir| = d_float` (about 420 at 400x225, 30°
  fov); a lens ray has `|dir| = focus_distance` (about 4 in the same scene).
  Since `kEpsilon` and `kMaxT` are expressed in units of `|dir|`, turning the
  lens on shifts the effective near clip by ~100x. It is harmless today —
  `kMaxT` is `FLT_MAX` and the epsilon stays sub-micron on a primary ray that
  starts in empty space — but **step 8's slab test must not normalize**, and
  any future epsilon tuning has to hold for both cases.

**Known artifact, not a bug.** The far shot's out-of-focus near sphere is
visibly blotchy. Its circle of confusion is ~36 px, so a single pixel's rays
genuinely disagree — some hit the sphere, some miss it entirely — and variance
in an estimated mean is what noise is. An in-focus pixel's rays all strike
nearly the same point and agree. Same 256-ray budget, a far harder integral.

---

### Step 8 — BVH

| | |
|---|---|
| **Goal** | Median split, then SAH, flattened to a linear array. Traversal written **twice — recursive, then iterative — both kept**, selectable at run time, so their difference is measured instead of asserted |
| **Done when** | You can state rays/sec before and after on the same scene, recursive against iterative on the same binary, and explain where the remaining time goes. Part 4 of the series is built from this data |
| **Status** | 🔨 **Core done 5 October.** Rungs A–C, ordering and SAH done; gate A verified 6 Oct. Open: heatmaps, figure script, a profiler-based recursive/iterative explanation, and the post-step-9 improvements in rung D |
| **Headline** | `macho-cows` (1 spp, one binary, 6 Oct): linear **4,725 ms** → recursive **24.7 ms**, iterative **23.2 ms**, **204×**. `cornell_box`: **940 s** linear (22 Sep) → **1.89 s** with SAH (6 Oct, a different day — see below); SAH 20% faster than median there. Recursive and iterative: **no measurable difference** with culling on |

**Sections of this step:** [the ladder](#the-ladder-progress) (progress) ·
[before-numbers](#the-before-numbers) · [instruments](#the-instruments-and-why-each-exists) ·
[results](#results) · [where the time goes](#where-the-time-goes) ·
[issues and fixes](#issues-and-fixes) · [design decisions](#design-decisions) ·
[after step 9](#after-step-9-rung-d-improvements)

#### The ladder (progress)

Ordering that makes it debuggable — do **not** skip the gates. Every rung that
changes speed gets a bench run and rows in `docs/data/step8-bvh.csv`, and that
list of rows is part 4's outline. Verify with `BVH_VERIFY=1` throughout: a BVH
that is merely slow still renders correctly; one that drops triangles makes
holes that are easy to miss by eye.

**A. Instruments** — on the linear path, before any tree.

- [x] `BVH_TRAVERSAL` switch, logged in frame totals
- [x] Traversal counts in locals, flushed per thread when its band ends — one
      add per call measured 6× slower ([issues](#issues-and-fixes))
- [x] Ray counters by kind
- [x] Build-stats line, `bench` record, `scripts/bench_bvh.sh`, `docs/data/`.
      Done 5 October; the CSV has rows from committed build `73f524d`.
- [x] **Gate:** `linear` on the new binary reproduces the 22 September
      triangle-test counts exactly — `macho-cows` 3,264,652,910, `rtiow_final`
      3,461,967,608. The counts are deterministic, so any other figure means
      the plumbing changed the linear path. **Passed 6 October** on the
      working-tree build (`RT_STATS=1`, 1 spp): both counts to the digit.
      `macho-cows` was also exact in every checkpoint run on 5 October.

**B. Recursive.**

- [x] `Build()` with a **median split** on centroid bounds, longest axis,
      through a recursive helper. It splits by count, half to each side, so
      coincident centroids cannot stall it — every level shrinks. Uses
      `std::nth_element` since `cf6998d` (6 October); `std::sort` before.
      *Prediction, written before any timing: median build time falls by a
      third to a half. Each level costs O(n) instead of O(n log n), but the
      bounds pass over every level stays, and the sort often got input already
      sorted by its parent along the same axis. Render time unchanged: each
      half holds the same faces unless centroids tie at the median, so counts
      should match to the digit, except perhaps on box meshes, whose centroids
      tie.* **Result:** build time fell 54–56% (a little more than predicted);
      render time unchanged within noise; but counts moved on five of six
      scenes, because centroid ties are common on every kind of mesh, not just
      boxes ([results](#results)).
- [x] `TraverseRecursive()`, **tightening `t_best` on every accepted hit** —
      where most of the speedup comes from. Done 5 October, written after the
      iterative one; it visits the near child first to match it, and shares
      the leaf test and the near-child choice with it.
- [x] **Checkpoint**, with `AABB::Hit()` returning `true`: every triangle is
      tested once per traversal, so the tree's triangle count must equal
      `linear`'s **to the digit** and `BVH_VERIFY` must stay silent. This
      proves the tree holds every triangle exactly once before culling exists
      to hide a bug. **Counts passed** on both scenes, both traversals. **The
      time criterion ("roughly unchanged") failed** — the unculled tree is
      1.6–1.9× slower; explained in [issues](#issues-and-fixes).
- [x] `AABB::Hit()` slab test with a precomputed `1/dir`, and **no**
      normalisation: the rest of the renderer carries unnormalized directions
      and `t` must mean the same thing everywhere. Both traversals compute
      `1 / ray.GetDirection()` once per call.
- [ ] Bench `linear` against `recursive` — the headline speedup — and the
      first heatmaps. **Partial:** CSV rows for `macho-cows`, `hier`,
      `instance` and `nonhier2`, re-taken 6 October at `8dbfffa` and
      `cf6998d` with the tree runs, 30 rounds ([results](#results)). Same-day
      linear rows for `cornell_box` and `rtiow_final`, and the heatmaps, are
      still to do.

**C. Iterative.**

- [x] At build time, check that `max_depth_` fits the traversal stack (64
      entries) and fail loudly if it does not
- [x] `TraverseIterative()`: an explicit fixed-size stack, near child first in
      both variants (originally "push right, then left")
- [x] **Gate:** node and triangle counts equal `recursive`'s to the digit, and
      the images hash identically, on every scene. **Passed 5 October** on
      `macho-cows`, `hier`, `instance`, `nonhier2` and `cornell_box` over 30
      rounds, `rtiow_final` over 5: one count and one hash per scene across all
      runs of both modes. `BVH_VERIFY` silent in 5 of 5 `recursive` runs of
      `macho-cows`. `final_animation` not run.
- [x] Prediction written down, then `recursive` against `iterative`,
      interleaved ([results](#results))
- [ ] Explain the result from a profiler on both, not a guess. A small gap is
      a finding too: say why. **Partial,** from the disassembly: GCC keeps
      only half the recursion — the near-child call is a real `call`, and the
      far-child call, in tail position, became a jump back to the top of
      `VisitNode`. So it is one call per interior node, at a depth of about
      11–13, which the return-address predictor handles well.

**D. Improvements**, each applied to both variants, each its own bench row.

- [x] **Front-to-back child ordering.** Each interior node stores its split
      axis, and the child on the ray's side of that axis is visited first.
      `macho-cows` nodes 4,193,917 → 4,069,473 (−3.0%), triangles 551,481 →
      493,144 (−10.6%), `BVH_VERIFY` silent; the rung C gate confirms the
      variants still agree.
- [x] **SAH split**, binned, 16 bins per axis. Triangle tests per ray fall
      32–53% on every scene ([results](#results)).
- [ ] The post-step-9 items: [children at the parent, a 32-byte node,
      leaf-ordered triangles, a wide BVH](#after-step-9-rung-d-improvements)

**Scope against the 10th.** A to C, plus ordering and SAH, are the step. The
optional items at the end of rung D are the first to cut.

**E. Trim the benchmark — done 6 October, before step 9.** The 6 October runs
took ~10 minutes for `cornell_box` alone (30 rounds × 4 configurations, then 30
counting rounds) and ~20 for `rtiow_final`, mostly re-measuring settled
questions. **Result: the same comparison — both scenes, median and SAH — now
runs in 63 s** and reaches the same conclusions (SAH 21% faster on
`cornell_box`, no difference on `rtiow_final`).

- [x] **Build time from every timed run; counts from two runs.** The first
      plan was to count once. It would have lost `build_ms`'s spread, which
      was only recorded in counting runs and was how `nth_element` was
      measured (−55% median build). So build timing became always-on — two
      clock reads per mesh at load — and every timed run records its own.
      Counts come from two `RT_STATS=1` runs per configuration: one to get
      them, one to check determinism (a destructor-based flush once gave
      wrong totals in 18 of 30 runs).
- [x] **Time `iterative` only by default.** Recursive against iterative is
      settled: three separate runs, within ~1–2%, inside noise. `recursive` is
      gated instead: one counting run per scene and split must match
      `iterative`'s image and counts to the digit.
- [x] **`linear` only on request** (`-m linear`). It is ~190× slower, and its
      before-numbers are recorded.
- [x] **`RT_SPP` and `-p`**, so slow scenes bench at a few spp: ms per
      pixel-sample is linear in spp (2.009× for 2×), and the fixed per-frame
      cost is down to ~2 ms.
- [x] **5 rotated rounds by default, and the median beside the mean.** The
      laptop throttles (see §6), and the median shrugs off the outliers.
- [x] **`ctest` times.** Serial 15.8 s; slowest are the BVH-vs-linear checks
      on the Lamborghini (2.5 s) and spaceship (1.9 s) meshes and the
      resolution test (1.8 s), all real coverage, so nothing is dropped.
      **Run it in parallel instead:** `ctest -j 8` takes 3.5 s. The documented
      commands now say so.

All gates held: default renders hash-identical to `HEAD` on `simple`,
`macho-cows`, `hier` and `nonhier2`; 40 of 40 tests; style clean.

#### The before-numbers

**Why they had to be taken first:** once the tree exists, the slow path can
only be measured with instruments that were in place before it. Taken **22
September 2026, this machine, on the guarded build** — the specular shadow-ray
guard ([issues](#issues-and-fixes)) is part of the baseline, so that the tree
is measured against it rather than credited with it.

**Do not compare against the 13 September figure of 3490 ms** (§6): the same
uninstrumented binary measured ~3960 ms on the 22nd, so the machine, not the
code, moved by about 7%. A comparison is only valid against numbers taken on
the same day as the after-numbers, or re-taken alongside them.

| Scene | Res | spp | pixel-samples | Triangles | Render | Triangle tests |
|---|---|---|---|---|---|---|
| `simple.lua` (5 spheres, no mesh) | 256x256 | 1 | 65,536 | 0 | 17 ms | 0 |
| `macho-cows.lua` | 256x256 | 1 | 65,536 | 17,530 | **4067 ms** | **3,264,652,910** |
| `rtiow_final.lua` (balls and boxes) | 480x270 | 1 | 129,600 | 9,064 | **6087 ms** | **3,461,967,608** |
| `cornell_box.lua` (car, drone, ship) | 400x400 | 32 | 5,120,000 | 21,084 | **940,119 ms** | **685,905,974,124** |

Means of three: `simple` 18/17/17, `macho-cows` 3896/4058/4248, `rtiow_final`
6064/5970/6228. The Cornell box is a single run, at 15m40s. `rtiow_final`'s
row is at 1 spp: its scene file defaults to `SAMPLES = 8` × `LENS_SAMPLES = 4`,
so reproduce it from a copy with both set to 1.

One row is dropped rather than re-taken: `keyblade.obj` was a probe with no
scene file behind it (256x256, 1 spp, 43,354 triangles, 17,062 ms,
15,917,334,392 tests). It is not reproducible from the repository, so it is not
a baseline.

**The derived quantities — why they exist:** wall-clock alone cannot separate
"the tree is working" from "the machine was busy"; these can.

| Scene | tests / pixel-sample | scans / pixel-sample | M tests/s | ms / pixel-sample |
|---|---|---|---|---|
| `macho-cows` | 49,815 | 2.84 | 803 | 0.0621 |
| `rtiow_final` | 26,713 | 2.95 | 569 | 0.0470 |
| `cornell_box` | 133,966 | **6.35** | 730 | 0.1836 |

- **tests / pixel-sample** = triangle tests / (width x height x spp). Divide
  that by the scene's triangle count and you get
- **scans / pixel-sample**: how many times an average pixel-sample scans a
  whole mesh. It is the ray count per sample in disguise — one primary ray,
  plus a shadow ray per hit, plus bounces to the depth cap. `macho-cows` at
  2.84 is a scene whose rays escape to the sky quickly; `cornell_box` at
  **6.35** is one where they cannot, because the room is closed. That 2.2x is
  the whole reason to keep a closed scene in the set.
- **M tests/s** is the machine's throughput, and it is **not constant**: it
  runs 569 to 803 across these three, because bigger frames and bigger meshes
  fall out of cache. Extrapolating a time from *another* scene's rate is worth
  about 30 per cent; extrapolating within one scene is good to about 1 per
  cent.
- **ms / pixel-sample** is the figure to scale a render time by. Doubling spp
  doubles it, and that held on the pre-guard build: the Cornell box took
  532,435 ms at 16 spp and 1,069,428 ms at 32, a factor of 2.009.

**Why these scenes, and why both comparison scenes stay.** `rtiow_final` and
`cornell_box` are deliberately opposite, and the tree should move them by
different amounts:

- `rtiow_final` is **open**, and most of its primitive count is 238 separate
  12-triangle box meshes. A per-`Mesh` tree gives each of those its own
  trivial tree and does nothing for the linear walk over 460-odd scene nodes.
  Expect a modest gain, dominated by the ship's 6,208 triangles.
- `cornell_box` is **closed**, and its 21,084 triangles sit in three meshes a
  ray bounces between until `max_depth` stops it. Almost all of the cost is
  inside meshes, which is exactly what the tree indexes. Expect the large gain
  here.

Per pixel-sample the Cornell box costs **3.9x** what `rtiow_final` does
(0.1836 ms against 0.0470). That ratio, not the raw wall-clock, is the honest
comparison — the two run at different resolutions and sample counts.
`assets/scenes/macho-cows.lua` remains the primary comparison scene: it is the
one with a published history.

Reproduce any row with:

```bash
RT_STATS=1 RT_LOG=info,geom:debug BVH_TRAVERSAL=linear ./build/raytracer assets/scenes/macho-cows.lua
```

Quality renders, kept as the visual before-state:
`docs/images/step8-baseline-rtiow.png` (480x270, 192 spp, **21m31s**
unguarded) and `docs/images/step8-baseline-cornell.png` (400x400, 32 spp,
**17m49s** unguarded, **15m40s** guarded). Neither was re-rendered, because
neither changed: the guarded build reproduces both bit for bit. Both make the
case for this step from opposite directions: the first has 6,208 ship
triangles with no bounding-volume early-out, so every ray that reaches the sky
pays for all of them; the second has no sky to reach.

#### The instruments, and why each exists

| Instrument | Why it exists | State |
|---|---|---|
| `BVH_TRAVERSAL=linear\|recursive\|iterative` | The machine drifts ~7% between days, so the before-number is re-taken alongside every after-number — inside one binary, so builds are not also being compared | Done; unset means `iterative` since 5 Oct |
| Triangle counter on the linear path | Without it the linear scan read 0 tests, and the data half of the comparison did not exist | Done 22 Sep |
| Node and triangle counters in the tree | Deterministic counts see changes wall-clock cannot | Done; per-thread tallies |
| `RT_STATS=1` | Counting cost 17–20%; timed runs must not pay it | Done; opt-in |
| Ray counts by kind | The exit criterion asks for rays/sec, and traversal calls are not rays | Done |
| Build-stats line | Build time falls outside `done in N ms`; without this line it is reported nowhere | Done |
| `bench` record + `scripts/bench_bvh.sh` | One grep-able line per run; interleaved, rotated, hash-checked rounds | Done |
| `BVH_HEATMAP=<path>.png` | Shows *where* the tree spends its work | To do |
| `scripts/bvh_figure.py` | Regenerates the post's charts from the CSV | To do |

**One binary, three paths.** `BVH_TRAVERSAL` is read once, like `BVH_VERIFY`.
`linear` forces `LinearScan` even when the tree is built. The mode is logged in
the frame-totals line, so no log is ambiguous about which path produced it. The
bench script always sets it explicitly. Before the default switched to
`iterative` on 5 October, `linear` and `iterative` wrote byte-identical images
on `macho-cows`, `hier`, `instance` and `nonhier2`, 3 rounds each.

**Counters that do not perturb what they count.** One `fetch_add` per node
visited, from 20 threads onto one cache line, would cost about as much as the
box test being counted. The plan was to count into locals and add once per
traversal; that turned out not to be enough either — see the counter entries in
[issues](#issues-and-fixes). **Decision (5 October): counting is opt-in.**
`RT_STATS=1`, read once before `main` (`src/core/stats.h`), turns on every
render-path counter. With it off, no counter touches `thread_local` or shared
state, and the frame-totals line says `counts off` instead of printing zeros.
Timed runs leave it off; counts come from a separate run with it on — they are
deterministic, so one counting run per configuration is enough. One binary,
interleaved, flag off vs on: `macho-cows` 41.8 vs 47.8 ms (30 rounds),
`cornell_box` 2534 vs 3183 ms (10 rounds). Counts with it on were identical in
every run.

**Ray counts.** Each band counts into its own struct, with no thread-local
access, and adds it to the frame totals once. `RT_LOG=render:debug` prints
`rays: primary, shadow, bounce, total`, and the BVH frame totals gained `calls`
(mesh queries, in every mode). Primary rays equal width x height x spp exactly:
65,536 on `macho-cows`, 5,120,000 on `cornell_box`. Rays and calls are
identical in linear, recursive and iterative, and identical across 30 runs.
`macho-cows` makes 184,715 rays and 3,140,155 mesh queries, 17 per ray: plane,
buckyball, three cows and twelve boxes. Divide by rays, not calls, for
nodes-per-ray and tests-per-ray.

**Build stats**, one line per mesh at load, on the existing `geom` debug line
for OBJ meshes (the 12-triangle `NonhierBox` meshes do not log): build ms, node
count, leaf count, max depth, mean and max triangles per leaf. `cow.obj`: 5804
faces, 4095 nodes, 2048 leaves, depth 11, 2.83 triangles per leaf (max 3),
built in 2.46 ms (sd 0.07, 30 runs), against a ~45 ms render. Leaves equal
(nodes + 1) / 2 on all nine meshes, as a tree where every interior node has two
children requires. `Build()` also logs an error if the leaves' face total
differs from the mesh's; that check always runs. Timing and stats are behind
`RT_STATS=1`.

**One machine-readable line per run.** A single `bench` record with everything
a row needs: date, commit, scene, resolution, spp, threads, traversal mode,
split, leaf size, build ms, render ms, rays by kind, nodes visited, triangles
tested. **`scripts/bench_bvh.sh`** runs the matrix and appends to
`docs/data/step8-bvh.csv` — under `docs/` because `renders/` is ignored and the
data has to survive until the post. Its rules, each there because breaking it
once produced a wrong number:

- **Interleaved, not blocked,** and **rotated** each round, so drift lands on
  every configuration equally.
- **Hashes checked every run:** within one split, every mode must write a
  byte-identical image. A mismatch aborts the run.
- **Times and counts from separate runs:** timed runs leave `RT_STATS` off and
  each records its own `build_ms` — build timing is always on since rung E,
  so build time keeps a per-round spread. Counts come from **two**
  `RT_STATS=1` runs per configuration: one to get them, one to check they are
  deterministic. *(For the 6 October rows, `8dbfffa` and `cf6998d`: one
  counting run per configuration per round, which is where `build_ms` came
  from then. The script at `cf6998d` has that version.)*
- **Recursive is gated, not timed,** unless `-m` asks for it: one counting run
  per scene and split must match `iterative`'s image and counts to the digit.
- **Slow scenes run at a few spp** with `-p` (`RT_SPP`): ms / pixel-sample is
  linear in spp (2.009x for 2x), so a low-spp run compares configurations as
  well as a full one. This replaces the old 200x200 Cornell probe.

**`scripts/bvh_summary.py`** turns the CSV into every table in
[results](#results); its docstring defines each figure (sample sd, Mrays/s,
per-ray counts, the paired ratio, the drift control).

**Still to build:**

- **The heatmap.** `BVH_HEATMAP=<path>.png`: for each pixel, the nodes visited
  by its primary rays, false-coloured on a log scale, with a second image for
  triangle tests. Read the thread's local counters before and after the
  primary ray and take the difference. Use **one fixed colour scale** across
  every mode and scene, and state its range in the caption: heatmaps each
  normalised to their own maximum all look alike, whatever they show. The
  linear scan's triangle-test heatmap is the control — flat, with every pixel
  paying for every triangle. (`RT_VIEW` in `src/render/debug_view.h` is the
  natural home for it.)
- **The figure script.** `scripts/bvh_figure.py` reads the CSV and draws the
  post's charts, the way `scripts/malley_figure.cc` regenerates the Malley
  post's figure. Two charts: render ms per scene per mode, with the spread;
  and tests-per-ray per mode on a log axis, since the linear scan sits in the
  tens of thousands and the tree should not.
- **Capture the broken one.** The first `BVH_VERIFY` mismatch drops triangles
  and leaves holes. Render the frame before fixing it (see the publishing plan
  in `ENGINEERING.md`).

#### Results

Every table here comes from `docs/data/step8-bvh.csv` through
`scripts/bvh_summary.py`; the command is given with each. Times are ms, mean
(sample sd), counting off, 20 threads.

**Linear against the tree** — the headline. `bvh_summary.py --commit
8dbfffa`: 6 October, 1 spp, median split, one binary, 30 interleaved and
rotated rounds per configuration. Speedup is the geometric mean of the
per-round linear ÷ iterative ratios; the tree was faster in 30 of 30 rounds on
every scene.

| Scene | linear | recursive | iterative | rays/s, linear → iterative | speedup |
|---|---|---|---|---|---|
| `macho-cows` | 4,725 (504) | 24.7 (5.4) | 23.2 (3.3) | 39.1 k → 7.96 M | **204×** |
| `nonhier2` | 80.5 (6.9) | 33.3 (4.0) | 32.6 (4.9) | 8.67 M → 21.4 M | 2.49× |
| `instance` | 38.5 (5.0) | 18.4 (3.3) | 18.7 (4.4) | 4.65 M → 9.58 M | 2.09× |
| `hier` | 22.0 (4.5) | 12.9 (3.3) | 12.2 (2.2) | 10.7 M → 19.4 M | 1.80× |

The 5 October rows (`73f524d`, 20 rounds) gave 138×, 2.2×, 1.7× and 1.5×.
Those included the ~11 ms background decode inside the timer, fixed since
([where the time goes](#where-the-time-goes)). A fixed cost added to both
sides shrinks every ratio, `macho-cows` most: (4,725 + 11) ÷ (23.2 + 11) is
138.

The gap between `macho-cows` and the rest is the per-mesh design showing: the
others are scene-graph-bound, with little inside their meshes for a tree to
skip. Triangle work on `macho-cows`, same day, same binary (~46 ms tree against
~3660 ms linear):

| mode | nodes visited | triangles tested |
|---|---|---|
| linear | 0 | 3,264,652,910 |
| iterative, left then right | 4,193,917 | 551,481 |
| iterative, near child first | 4,069,473 | 493,144 |

**Recursive against iterative.** *Prediction, 5 October, before any timing:
iterative is faster, because its explicit stack replaces the call frame
recursion pushes for every node visited. No magnitude given.* **Result: no
measurable difference with culling on.** ms, mean (sd), 30 rounds unless noted:

| Scene | recursive | iterative |
|---|---|---|
| `macho-cows` | 52.1 (5.2) | 51.0 (4.9) |
| `hier` | 24.9 (1.6) | 24.5 (1.6) |
| `instance` | 40.9 (2.8) | 42.5 (4.7) |
| `nonhier2` | 46.7 (2.4) | 47.6 (3.4) |
| `cornell_box` | 3837 (433) | 3798 (493) |
| `rtiow_final` (5 rounds) | 46,536 (1302) | 46,551 (2887) |

Iterative is ahead on three scenes and behind on three. With culling off on
`macho-cows`, where the walk dominates (30 rounds): recursive 9422 ms (950),
iterative 9269 ms (896); the paired difference is +153 ms (sd 1030), and
recursive was slower in 20 of 30 rounds. At most a ~1–2% edge for iterative,
not separable from noise. The prediction's direction may hold; its size is
lost in noise wherever culling is on. The disassembly explains why (rung C).

Re-run 6 October inside the SAH bench (`bvh_summary.py --commit 8dbfffa`, 30
rounds, rotated). Iterative ÷ recursive, geometric mean of the per-round
ratios, with the rounds iterative won:

| Scene | median split | SAH split |
|---|---|---|
| `macho-cows` | 0.950 (18/30) | 1.004 (14/30) |
| `hier` | 0.960 (15/30) | 0.968 (17/30) |
| `instance` | 1.004 (12/30) | 0.911 (22/30) |
| `nonhier2` | 0.974 (17/30) | 1.002 (17/30) |
| `cornell_box` | 0.988 (21/30) | 0.972 (25/30) |
| `rtiow_final` (5 rounds) | 1.044 (1/5) | 1.049 (1/5) |

Same verdict. Within 5% either way except `instance` under SAH (0.911), and
no sign that holds across scenes:
`cornell_box`, the scene where traversal dominates, leans iterative by 1–3%,
while `rtiow_final` leans recursive over only 5 rounds.

**SAH against median.** Binned, 16 bins per axis; the cost is triangles ×
surface area per side, falling back to the median when every centroid
coincides. `BVH_SPLIT=median|sah` selects it at run time; median is still the
default. Trees are valid on all nine meshes, max depth 16.

`bvh_summary.py --commit 8dbfffa`: 6 October, median still split by a full
sort, iterative, 30 interleaved and rotated rounds (`rtiow_final` 5). Each
scene at its own resolution and spp (`cornell_box` 400x400, 32 spp). SAH ÷
median is the geometric mean of the per-round ratios.

| Scene | triangles/ray, median → SAH | nodes/ray | median | SAH | SAH ÷ median | SAH won |
|---|---|---|---|---|---|---|
| `cornell_box` | 8.505 → 4.461 (−47.5%) | 24.14 → 20.58 | 2,349 (304) | **1,887 (248)** | **0.803** | 29/30 |
| `hier` | 2.088 → 1.360 (−34.8%) | 6.46 → 5.62 | 12.20 (2.15) | 11.18 (2.09) | 0.916 | 22/30 |
| `instance` | 1.366 → 0.901 (−34.0%) | 13.40 → 13.53 | 18.66 (4.37) | 18.14 (4.79) | 0.968 | 21/30 |
| `nonhier2` | 0.599 → 0.277 (−53.8%) | 2.63 → 2.65 | 32.57 (4.88) | 32.19 (4.25) | 0.991 | 14/30 |
| `macho-cows` | 2.670 → 1.813 (−32.1%) | 22.03 → 21.22 | 23.20 (3.26) | 23.17 (3.28) | 0.998 | 16/30 |
| `rtiow_final` | 3.034 → 1.527 (−49.6%) | 242.49 → 242.44 | 23,069 (2,005) | 23,199 (1,632) | 1.007 | 2/5 |

`cornell_box` goes from 13.9 to 17.2 Mrays/s. Recursive gives the same
picture (`cornell_box` 0.816, SAH won 29/30; `hier` 0.908, 22/30).

**What this says.** SAH cuts triangle tests by a third to a half on every
scene, but the clock follows only where triangle tests are a large share of
the frame: `cornell_box` (−20%) and `hier` (−8%). Elsewhere the ratio is
within a few % of 1, with SAH winning about half the rounds. Those frames go
to the scene graph, not to triangles: `rtiow_final` asks 239 meshes per ray
and visits 242 nodes per ray under either split
([where the time goes](#where-the-time-goes)). SAH is never measurably
slower.

**SAH also builds faster than the sorted median.** Scene totals over 30
counting runs: `macho-cows` 2.26 ms (sd 0.13) against 2.58 (0.18);
`cornell_box` 8.42 (0.57) against 10.12 (1.07). Binning costs O(n) per level;
the sort cost O(n log n). On the cow SAH builds fewer nodes (3,665 against
4,095) but a deeper tree (15 against 11). This replaces the "7.2 ms against
4.4 ms" quoted here before, which the per-round build times do not
reproduce.

**Images.** SAH and median write the same image on five scenes. On
`cornell_box` they differ (`e637150c` against `e8fae1a6`); see the correction
below.

**Median by `nth_element` instead of a full sort (`cf6998d`).** The
prediction is in [rung B](#the-ladder-progress). `bvh_summary.py --before
8dbfffa --after cf6998d --control sah`. The two benches ran back to back on 6
October with the same matrix, each from its own git worktree, so no other
build could replace the binary mid-run ([issues](#issues-and-fixes)). They are
not interleaved, so they carry the drift between them: SAH, whose code did not
change, moved 0.85–1.06× between the runs. SAH is therefore the control, and
"÷ control" divides a median ratio by SAH's in the same scene and traversal.

Build time, the scene's meshes in total, ms, iterative rows, 30 counting runs
(`rtiow_final` 5):

| Scene | sort | `nth_element` | after ÷ before | ÷ control |
|---|---|---|---|---|
| `macho-cows` | 2.578 (0.183) | 1.141 (0.029) | 0.443 | 0.434 |
| `cornell_box` | 10.119 (1.072) | 4.511 (0.320) | 0.446 | 0.441 |
| `rtiow_final` | 2.999 (0.024) | 1.662 (0.250) | 0.554 | 0.571 |

The recursive rows build the same trees and agree (0.451, 0.456, 0.489). So
the median build is 54–56% faster, a little beyond the predicted third to a
half (45–51% on `rtiow_final`, 5 runs), and it now takes about half SAH's time
(`macho-cows` 1.14 against 2.30 ms, `cornell_box` 4.51 against 8.52). The
other three scenes' meshes build in under 0.03 ms, too short to compare.

**Render time: unchanged within noise.** ÷ control, iterative / recursive:
`cornell_box` 0.991 / 1.001, `macho-cows` 0.981 / 0.951, `nonhier2` 0.970 /
0.986, `hier` 1.034 / 1.009, `rtiow_final` 0.979 / 1.044. `instance`, whose
tree did not change at all, reads 0.984 / 1.058, so ±6% is this method's floor
on the small scenes. Without the control the raw ratios ran 0.87–1.10.

**Counts: the prediction was wrong.** They moved on five of six scenes
(median split; both traversals identical):

| Scene | nodes visited, sort → `nth_element` | triangles tested | image |
|---|---|---|---|
| `hier` | 1,527,558 → 1,576,984 (+3.2%) | 493,928 → 602,002 (+21.9%) | same |
| `nonhier2` | 1,836,334 → 1,846,320 (+0.5%) | 418,081 → 444,899 (+6.4%) | same |
| `rtiow_final` | 2,958,977,539 → 2,959,272,645 (+0.01%) | 37,018,412 → 37,789,694 (+2.1%) | same |
| `macho-cows` | 4,069,473 → 4,060,789 (−0.2%) | 493,144 → 493,441 (+0.06%) | same |
| `cornell_box` | 785,339,570 → 786,055,350 (+0.09%) | 276,695,380 → 276,625,493 (−0.03%) | changed, now SAH's |
| `instance` | 2,397,050, unchanged | 244,266, unchanged | same |

The cause is ties at the median, and it was tested rather than assumed. With
the comparator breaking ties by face index, `(centroid, face)`, `std::sort`
and `std::nth_element` gave identical node and triangle counts to the digit on
the five scenes tried (all but `rtiow_final`). Without a tie-break, which of
the faces sharing the median centroid go left is up to the algorithm, and
their boxes set how much the two halves overlap. The tie-broken counts are a
third set again (`hier`: 544,552 triangles), so `hier`'s +21.9% is tie order,
not a property of `nth_element`. Node, leaf and depth figures were identical
on every mesh; `BVH_VERIFY` was silent on all five scenes with the new tree,
including the full `cornell_box` frame; `bvh_test` passed with the same six
grazes.

**The `cornell_box` image, and a correction.** The new median tree renders
exactly SAH's image (`e637150c`), with SAH's ray counts (32,532,034 against
sort's 32,532,061), and `BVH_VERIFY` finds no mismatch in it. So the
difference between the median and SAH images was **not** the graze that
`BVH_VERIFY` flagged under SAH, as [issues](#issues-and-fixes) said: a tree
with no such mismatch changed the image the same way. `BVH_VERIFY` compares
hit and `t` (to 1e-4) per mesh query, not which triangle won, and a
disagreement on a mesh behind another object never reaches the image. The
difference is consistent with two triangles hit at the same `t`, or within
1e-4, with the tree deciding which one wins. That has not been confirmed ray
by ray.

#### Where the time goes

**Why profile:** the exit criterion asks to *explain* the remaining time, not
just shrink it. No sampling profiler is installed, and the Windows tools cannot
read MinGW symbols, so the profile came from a minimal sampler,
`scripts/sample_profile.cc` with `scripts/symbolize_profile.py`. It suspends
each render thread every few ms, reads its instruction pointer and maps it
through `nm`. Three runs agreed to within about half a percentage point, ~3,900
worker samples each.

Share of worker samples, after the two fixes the profile found
([issues](#issues-and-fixes)):

| | `macho-cows` 64 spp | `cornell_box` |
|---|---|---|
| `TraverseIterative` (box tests inlined) | 23% | 43% |
| `IsTriangleIntersection` | 3.5% | 14% |
| scene graph (`SceneNode`, `GeometryNode`, `ToLocal`/`ToWorld`) | 38% | 18% |
| `Mesh::IsHit` dispatch | 6% | 6% |
| spheres | 5% | 1% |
| shading, sampling, `pow` via `exp2l`/`log2l`, environment | ~12% | ~5% |
| system DLLs | 8.5% | 8.5% |

**`macho-cows` is a scene-graph benchmark more than a BVH one.** Each ray asks
17 meshes in turn, each query transforms the ray into the mesh's space, and 1.3
nodes are visited per query, so most queries end at the mesh's root box.
Nothing culls a whole subtree: there is no top-level structure over the
instances, which is the shared-BVH half of step 12. In `cornell_box`, traversal
and triangle tests dominate, as a BVH benchmark should.

**A fixed cost per frame, separated from the per-ray cost.** Fitting
`macho-cows` at 1, 2, 4, 8 and 16 spp (10 interleaved rounds each) gives
12.9 ms fixed + 19.1 ms per spp. So 42% of the 1 spp frame did not depend on
rays at all, and per-ray comparisons belong at 16 spp or more. The fixed cost
was the background texture decode — fixed, see [issues](#issues-and-fixes).

#### Issues and fixes

| Date | Issue | Symptom | Cause | Fix | Result |
|---|---|---|---|---|---|
| 22 Sep | Shadow-ray far bound | `cornell_box` rendered black | Bound was `kMaxT`, but the unnormalized shadow ray puts the light at `t = 1` | Far bound `1.0` (step 10 note) | Existing scenes byte-identical |
| 22 Sep | NEE at specular hits | Wasted occlusion traversals | `Eval` is exactly 0 off the delta direction | Guard on `!IsSpecular()` | 9.0–11.3% fewer triangle tests, byte-identical |
| 22 Sep | Linear path counted 0 tests | No "before" data | Counter only in `Traverse` | `CountTrianglesTested()`, one add per scan | Free within noise |
| 5 Oct | Counting cost 5× the work | 225 ms vs 36 ms on `macho-cows` | One shared `fetch_add` per counter per call | Per-thread tallies | Still ~17–20% → made opt-in |
| 5 Oct | Destructor flush wrong | Totals short, or off by 2^32, in 18 of 30 runs | MinGW emulated TLS: unreliable thread-exit destructors | Explicit `FlushThreadStats()` | 30 of 30 exact |
| 5 Oct | Locks in the hot path | 18–22% of time in `pthread_*`, `__emutls_get_address` | `NonhierBox` ran `std::call_once` on every test | Build the box mesh in the constructor | 64 spp 1770 → 1242 ms |
| 5 Oct | Background decoded inside the timer | 12.9 ms fixed per frame | `Render` decoded the 3.3 MB PNG on every call | Decode once in `SetBackground` | 1 spp 31.8 → 20.9 ms |
| 5 Oct | Unculled tree slower than the scan | 1.6–1.9× slower, same triangle tests | Out-of-line triangle test, plus the walk itself | Explained, not fixed (culling makes it moot) | `-flto` closes ~45% |
| 5 Oct | Hit accepted at `t = NaN` | Any later triangle could "win" | Parallel ray; "outside the range" written as two comparisons lets NaN through | Checks accept only what is inside the range | No scene image changed |
| 5 Oct | One `BVH_VERIFY` mismatch under SAH | One mismatch in the full `cornell_box` frame | A graze 2 float steps outside a vertex; SAH's tighter leaf box rejects it | `tests/bvh_test.cc` classifies grazes | Not a bug in the tree. *Not* the cause of the 278-pixel median/SAH image difference, as first thought (6 Oct, [results](#results)) |
| 5 Oct | Bench order bias | Second-run config ~6% slow on 30 ms scenes; flipped `nonhier2`'s verdict | Fixed order within a round | Rotate the order every round | 5 Oct CSV rows may carry few-% bias |
| 6 Oct | Bench reported rows it never wrote | "appended 600 rows", CSV unchanged | The CSV was held open elsewhere ("Device or resource busy"); the append was unchecked and the exit trap deleted the rows | Check the append; on failure keep the rows in a temp file and exit non-zero | 600 rows re-taken |
| 6 Oct | A bench run died with no output | No `bench` record, `cornell_box` round 5 | Another session rebuilt `build/raytracer.exe` at that instant | Bench each commit from its own git worktree in `%TEMP%` | Not a renderer crash: 145 reruns, 0 failures |
| 6 Oct | Median counts moved with `nth_element` | `hier` +21.9% triangle tests | Ties at the median centroid, broken differently by `sort` and `nth_element` | None needed; a `(centroid, face)` tie-break makes the two identical | Build −54–56%, render unchanged |

**The specular shadow-ray guard, and why it landed before the tree.** Next
event estimation ran at *every* hit, including hits on mirror, metal and
dielectric. Those materials answer `Eval` with exactly zero for every direction
but their one, and a point light is never on it — so each such hit bought a
full occlusion traversal and multiplied the result by zero. The NEE loop is now
guarded by `!material->IsSpecular()`. Both binaries were built and run
alternately, so any drift in the machine landed on both:

| Scene | Triangle tests, unguarded | guarded | Removed |
|---|---|---|---|
| `macho-cows` | 3,264,652,910 | 3,264,652,910 | **0%** |
| `rtiow_final` | 3,904,109,528 | 3,461,967,608 | **11.3%** |
| `cornell_box` | 753,885,512,940 | 685,905,974,124 | **9.0%** |

`macho-cows` does not move at all, because it has no specular material; that
zero is the control. The Cornell box moves least of the pair despite being the
most specular scene — its metals are enclosed, so a ray that skips a shadow
test still goes on to bounce. **The output is byte-identical, and that is the
pass condition:** `Eval` returns exactly `vec3(0.0f)` and the loop draws no
random numbers, so neither the arithmetic nor the RNG stream shifts. Verified
at both ends of the range: the 200x200 4-spp probe and the full 400x400 32-spp
render hash the same before and after. Any difference at all would have meant
the guard was throwing away real light.

Two things this settles about method. **Wall-clock could not have found it.**
Across three interleaved probe runs each way the means were 29,647 and 28,986
ms — 2% apart, inside a ±12% spread — while the triangle counter returned the
identical figure to the digit on all three runs. When a change is a 9% one,
only the deterministic instrument can see it. **And it had to go in before the
baseline.** Had the guard landed after the tree, the tree would have collected
credit for the 9% as well, with no way left to separate them.

**The triangle counter had to be added before the tree.** `g_triangles_tested`
was only incremented inside `BVH::Traverse`, so on the linear-scan path it read
zero. `BVH::CountTrianglesTested()` takes one add per scan with the whole face
count — identical to a per-triangle count, since the scan tests every face
unconditionally. Measured against the uninstrumented build it costs nothing
above noise; if anything the instrumented build ran faster, which is how you
know the difference is the machine.

**Counting cost more than the work it counted.** On `macho-cows` the iterative
traversal took ~225 ms with one shared `fetch_add` per counter per call, and
~36 ms with the adds removed. Tallies moved into a `thread_local` struct, and
`RenderBand` calls `BVH::FlushThreadStats()` once before returning; the threads
are joined before `ReportStats`, so the totals are complete.
`CountTrianglesTested` uses the same tally, so linear and tree pay the same
counting cost. That build ran in ~51 ms against ~36 ms with no counting, so
the counters still cost ~15 ms — but from back-to-back runs, not interleaved.
Re-taken interleaved against a build with the tally lines removed: `macho-cows` 48.0 vs 39.4 ms (30 rounds, counted slower in 29),
`cornell_box` 2960 vs 2462 ms (10 rounds, slower in all 10) — ~17–20%. A third
build kept the per-node increments but never touched `t_stats`, and ran as fast
as no counting (gaps 8.4 ms and 527 ms against the full build). So the
increments are free, and the whole cost is the once-per-call `thread_local`
access, which MinGW emulates with a call to `__emutls_get_address`. Hence
`RT_STATS`. Every timing before that carried this overhead on both sides of
each comparison: the ratios hold, but those absolute tree times are ~17% high.

**A first flush, from the `thread_local`'s destructor at thread exit, was
wrong in 18 of 30 runs:** some totals came up short, and some were off by
multiples of 2^32, as if freed memory had been read. MinGW emulates TLS, and its
thread-exit destructors are not reliable. The flush is now an explicit call and
the struct has no destructor; 30 of 30 runs then agreed to the digit.

**Locks in the hot path, found by the profiler.** The first profile of
`macho-cows` (64 spp) found 18–22% of worker time in `pthread_spin_lock`,
`pthread_getspecific`, `__emutls_get_address` and `pthread_once`, with
`RT_STATS` off. The source was `NonhierBox::IsHit`, which ran `std::call_once`
on every test to build its mesh lazily; libstdc++ on MinGW routes every
`call_once` through emulated TLS and `pthread_once`. The mesh is now built in
the constructor, which also fixes a leak and moves the box builds out of the
render threads. Interleaved, counting off, images byte-identical on seven
scenes: 1 spp 37.6 → 30.4 ms (30 rounds, faster in all 30); 64 spp 1770 → 1242
ms (10 rounds, faster in all 10). Every `macho-cows` time before this carried
that overhead in every mode.

**The background was decoded inside the timer.** Main-thread samples put
`lodepng_inflatev` on top: `Render` decoded the 3.3 MB PNG inside its own
timer, on every call. With `gr.set_background` removed (15 interleaved rounds
at 1 and 16 spp), the fixed cost went from 11.4 ms to −0.6 ms, i.e. nothing; the
texture lookups add ~2 ms per spp on top. Fixed the same day: `SetBackground`
decodes once, outside the timer, and setting the same path again reuses it.
Interleaved, 15 rounds: fixed cost 13.1 → 1.9 ms, `macho-cows` 1 spp 31.8 →
20.9 ms (faster in all 15), per-spp cost unchanged. Images and logs unchanged,
except that a missing texture now logs its error at `gr.set_background`. The
1 spp rows in the CSV from `73f524d` predate this fix and include the decode.

**The unculled tree is slower than the scan.** The rung B checkpoint expected
time "roughly unchanged". Measured: `macho-cows` at 1 spp tested 3,264,652,910
triangles in both modes (3537 ms linear, 6264 ms tree); `rtiow_final` at 32 spp
tested 110,607,031,216 in both (200,047 ms linear, 316,212 ms tree), one run
per mode. Repeated on `macho-cows` over 30 interleaved rounds after the flush
fix: every tree run gave 3,264,652,910 triangles and 2,296,561,595 nodes, every
linear run 3,264,652,910. Time, mean (sd, min–max): linear 4720 ms (628,
3408–5493), tree 9099 ms (1207, 6848–10865); a noisy session, ratio of means
1.9×. So the tree is 1.6–1.9× slower while doing the same triangle tests, plus
2.3 billion (`macho-cows`) and 70.3 billion (`rtiow_final`) node visits.

A first guess blamed cache misses from the shuffled `indices_` order. It was
wrong: the cow's vertices, faces and nodes all fit in L2. Two causes measured
instead. (1) `LinearScan` inlines `Mesh::IsTriangleIntersection`, since both
are in `mesh.cc`, while `Traverse` in `bvh.cc` calls it out of line per
triangle. A `-flto` build, 10 interleaved rounds: tree 8862 → 6952 ms, linear
4634 → 4864 ms (noise) — closing ~45% of the gap. (2) The rest is the walk
itself: 2.3 billion pops, leaf tests and direction branches that culling would
normally pay for. With culling on, `-flto` changes nothing measurable (43.9 vs
44.4 ms, 30 rounds each), because only ~0.5 million triangle tests are left.
`recursive` re-run here, 30 rounds interleaved with `iterative`: both gave
3,264,652,910 triangles and 2,296,561,595 nodes in all 30, and the same image
hash.

**Two findings from SAH.** (1) `IsTriangleIntersection` accepted a hit at
`t = NaN` when a ray ran parallel to the triangle's plane: "outside the range"
written as two comparisons lets NaN through, and a NaN `t_best` then lets every
later triangle win. The checks now accept only what is inside the range. No
scene image changed. (2) The one `BVH_VERIFY` mismatch in full-size
`cornell_box` under SAH is a graze: the triangle test accepts a ray passing 2
float steps outside a vertex, and SAH's tighter leaf box rejects it. *This
was taken to be why SAH's `cornell_box` image differs from median's in 278
pixels. It is not: the `nth_element` median tree renders SAH's image with no
`BVH_VERIFY` mismatch at all ([results](#results), 6 October).*
`tests/bvh_test.cc` checks every split and traversal against a linear scan, and
counts a disagreement as a graze, not a failure, only when the hit triangle's
own box also rejects the ray.

**The bench's fixed order biased it.** Whichever configuration ran second
measured ~6% slow on 30 ms scenes, enough to flip `nonhier2` from "SAH 7%
slower" to "5.5% faster". The script now rotates the order every round.
Comparisons made with the fixed order, including the 5 October CSV rows, can
carry that bias at the few-% level.

#### Design decisions

**Why recursive first.** The recursive traversal is the algorithm written as
its own definition — test the box, descend into both children, keep the nearer
hit — so it is the version most likely to be right first time. Once it passes
`BVH_VERIFY` it becomes the reference the iterative version is checked against,
the role `LinearScan` plays for the tree. The iterative version swaps call
frames for an explicit stack of node indices: an optimisation, which needs
something correct to be compared with. *(In practice the ladder was climbed out
of order: the iterative traversal was written first. Both checkpoints and rung
C's gate have since been run against both.)*

**The invariant that makes the comparison clean.** If both traversals descend
into children in the same order, they visit the same nodes and test the same
triangles in the same sequence. Their counts must then agree **to the digit**,
and their images must hash the same; only the clock may differ. So a difference
in the counts is a bug in one of them, never a speedup — the same argument that
let the triangle counter see the 9% guard that wall-clock could not. Whenever
the child order changes, it changes in both.

**`Build()` stays recursive.** It runs once per mesh at load, outside the
`done in N ms` timer, and a median split bottoms out around depth 13 even for
all 21,084 of the Cornell box's triangles in one mesh
(log2(21,084 / 4) ≈ 12.4), so there is no call stack to overflow. An iterative
build buys nothing a benchmark can show unless the build-stats line says
otherwise; it is the last, optional item in rung D.

#### After step 9: rung D improvements

Each is applied to both traversals, gets a written prediction first, and gets
its own bench row. **Every number so far is at 20 threads on 14 cores:** each
P-core's two hyper-threads share its L1 and L2, and each 4-E-core cluster
shares one 2 MB L2, so a thread's effective cache is smaller than the per-core
figures below. Bench the layout items with `RT_THREADS` (added 6 October) at 1
thread (the per-core cache effect, isolated) and at 20 (throughput, including
contention for the shared L3 and memory). A layout change may matter more at 20
than at 1.

- [ ] **Test the children at the parent.** *Proposed 6 October.* Pop only
      nodes already known to be hit. At an interior node, test both children's
      boxes, push only those that hit, and push the farther one first, ordered
      by the two `t_near` values rather than the split-axis sign. Push each
      child together with its `t_near`, and when it is popped, drop it without
      touching the node if `t_best` has shrunk below that `t_near`. Box tests
      stay about the same (every child of a visited node is still tested once);
      what should fall is pushes, pops and nodes loaded. Define the counters
      before predicting: "nodes visited" now means popped, so add a box-test
      count beside it. A missed child's node is still read for its box until
      nodes hold their children's boxes — the wide BVH's layout, below; this is
      its binary form.
- [ ] **A 32-byte node, two per cache line.** Measured 5 October: `BVHNode` is
      44 bytes, so on this i7-12700H (64-byte lines; L1d 48 KB per P-core,
      32 KB per E-core; L2 1.25 MB per P-core; L3 24 MB) a node often
      straddles two lines. The pre-order build makes `left_child` always
      `i + 1`, so drop it. Put `right_child` (interior) and `first_index`
      (leaf) in a union, and make `index_count` a `uint16_t` and `split_axis` a
      `uint8_t`: 24 + 4 + 2 + 1 + 1 pad = 32, `alignas(32)`, which is pbrt's
      `LinearBVHNode`. Counts and hashes must not move; only the clock may.
      Prediction to write down first: the cow's whole working set is ~380 KB,
      L2-resident, so expect a modest gain (5–15%), larger on bigger meshes.
- [ ] **Leaf-ordered triangles.** *After the node.* A leaf test now reads
      `indices_[k]`, then `faces[f]` (3 x `size_t`, 24 bytes), then three
      scattered vertices: up to five lines for 36 bytes of data. After the
      build, copy each triangle's three corners in leaf order, so a 4-triangle
      leaf is 2–3 consecutive lines with no lookups through other arrays.
- [ ] **A wide BVH, 4 or 8 children per node, with a SIMD slab test.** *After
      the leaf-ordered triangles.* Build the binary SAH tree as now, then
      collapse it by pulling grandchildren up into their grandparent. Store a
      node's child boxes as arrays (`min_x[4]`, `min_y[4]`, ...) so one SSE
      (4-wide) or AVX2 (8-wide) instruction tests every child at once; this
      machine has AVX2, not AVX-512. Depth falls from log2 N to log4 N or log8 N
      (the cow: ~6 levels instead of 11–15), so fewer stack operations and
      dependent loads. This is the shape Embree builds, and so what Cycles
      traverses on the CPU. Widths are powers of two because SIMD lanes are; a
      3-wide node would leave a lane idle on every test. The gate changes:
      visit order differs from the binary tree, so node counts will not match.
      `BVH_VERIFY` must stay silent. The image should hash the same — the
      leaves are unchanged and collapsing only removes intermediate boxes, so
      it can never cull more. A differing pixel is acceptable only as an exact
      tie (two triangles at the same `t`, broken by visit order), as between
      splits.
- [ ] *Optional:* a `kLeafSize` sweep (1, 2, 4, 8, 16). It is a compile-time
      constant today; make it an environment override first so the sweep runs
      on one binary.
- [ ] *Optional:* iterative `Build()`, only if the build-stats line shows build
      time is a meaningful fraction of render time.

---

### Step 9 — Textures

| | |
|---|---|
| **Goal** | Procedural checker first — it makes UV seams and winding errors visible instantly. Then image textures with bilinear sampling. Normal maps last |
| **Done when** | A textured **OBJ** model matches a reference render, and you understand why your first normal map attempt looked wrong |
| **Status** | 🔨 Started 6 October; piece 1a next |
| **Cut first** | Normal maps |

**Restated from glTF deliberately.** There will be no glTF loader by the
deadline, and scoring this step against a loader that is out of scope would
drag step 7 back in through the back door. The OBJ path is the reference
instead.

*Where you stand:* nothing yet. No `vt` parsing, no UV in the hit record, no
sampler. lodepng is already vendored, so image loading is solved. The first two
are what the restated criterion actually costs — they are step 9's work now,
not step 7's. `RT_VIEW` (6 October) is ready for the `albedo` and `uv` views
this step needs: `src/render/debug_view.h` says where each goes.

#### The pieces

Each piece has one new idea and one check. Commit after each gate.

| # | Piece | Check |
|---|---|---|
| 1a | Pass the `HitRecord` into the BSDF (option A below) | Byte-identical renders; `bsdf_test` numbers unchanged |
| 1b | `Texture` / `SolidColor`; Lambertian reads its albedo from a texture | Byte-identical again; add `RT_VIEW=albedo` |
| 2 | Spatial checker, from the 3D hit point — no UVs needed | A checkered floor |
| 3 | UV in `HitRecord`, sphere UVs, UV checker | The checker wraps a sphere; add `RT_VIEW=uv` |
| 4 | OBJ `vt` parsing, barycentric UVs (`IsTriangleIntersection` must output the barycentrics) | The checker on a mesh shows its seams |
| 5 | Image texture, bilinear sampling | A textured sphere |
| 6 | **Exit:** a textured OBJ against a reference render | Done |

#### How a texture reaches the material

A texture needs to know where the hit is: the point now, the UV later. `Eval`,
`Pdf` and `Sample` receive only directions and a normal, so they cannot ask.
Two ways to fix that were weighed on 6 October.

- [ ] **A: pass the `HitRecord` into the BSDF.** *Now; this is step 9.*
      `Eval`, `Pdf` and `Sample` take the hit record in place of `normal`,
      which it already carries. That is the book's `scatter(r_in, rec, ...)`.
      It is about 30 mechanical edits: 18 overrides (6 materials x 3) and
      14 call sites in `integrator.cc`, `bsdf_test.cc` and `bsdf_probe.h`,
      where a `HitWithNormal(n)` helper keeps the tests reading as before.
      Two gates, each byte-identical:
      - **1a, the signature alone.** Watch one trap: `integrator.cc`
        normalises the normal into a local, but the record still holds the
        primitive's unnormalised one. Write the normalised normal back with
        `record.SetNormal` before the BSDF is called, or every shade
        changes. `bsdf_test`'s numbers must not move.
      - **1b, `Texture::Value(const HitRecord&)`.** A `SolidColor` texture,
        and `LambertianMaterial` holding a `std::shared_ptr<Texture>` where
        `albedo_` was; Lua's `kd` becomes a `SolidColor`. `Value` takes the
        whole record so that adding a UV in piece 3 changes no texture
        signature. Add `RT_VIEW=albedo` here.
- [ ] **B: resolve the material at the hit, then evaluate by direction.**
      *Later, after the deadline; not step 9.* The integrator first asks the
      material for its BSDF at this hit, `material->At(hit)`, with every
      texture already looked up, and `Eval`/`Pdf`/`Sample` go back to
      directions only, so the BSDF tests need no hit record at all. It is
      more new code than A, which is why A ships now. It is here because it
      is how production renderers are built: pbrt's `Material::GetBSDF`
      returns a BSDF for one shading point, and Cycles evaluates the shader
      graph at the hit into closures before any closure is sampled. Doing it
      is the way to see that split from the inside, and it is the answer to
      "how would you structure this at scale?" The gate is the one A met:
      byte-identical renders, unchanged `bsdf_test` numbers.

---

### Step 12 — Instancing + motion blur

| | |
|---|---|
| **Goal** | Transform-instanced geometry sharing one BVH; rays carrying time; transforms interpolated over the shutter interval; many instanced spheres with per-instance motion |
| **Done when** | An animated multi-frame sequence renders with correct blur |
| **Status** | ◇ Attempt only once steps 5, 8 and 9 have passed |

If it is attempted, build the motion-blur half: time on the ray, transforms
interpolated across the shutter. The shared-BVH instancing path can wait.
Nothing in the blur work is blocked on it — scene-graph instancing already
works, and the two share a heading only because they were planned together.

*Where you stand:* better than you might expect.
`assets/scenes/instance.lua` already reuses a shared subtree under several
parent transforms, so scene-graph instancing works. The animation pipeline
exists — `assets/scenes/final_animation.lua` drives 85 CSV keyframes through
`gr.render` and `scripts/stitch_animation.sh` turns the frames into a video.
Missing: time on the ray, transform interpolation, and a shared-BVH instancing
path. `assets/scenes/test.lua` has a commented-out `gr.nh_sphere_mb` carrying a
velocity vector — the original idea.

---

## 5. Deferred past the deadline

Steps 6, 7, 10 and 11 are out of scope for the first pass and are parked here so
the sequence above reads as the work actually queued. The renderer is
demonstrably weaker without them — this records what is being given up, not
that it does not matter.

### Step 6 — Multithreading over tiles

| | |
|---|---|
| **Goal** | Tile-based job queue, per-thread RNG state (never share a generator), one shared accumulation buffer |
| **Done when** | Output is bit-identical to single-threaded at a fixed seed, and scaling across thread counts is measured. Expect it to be sublinear — find out why |
| **Cost of deferring** | 16 threads measured 2.05× on a 14-core, 20-thread machine, and step 8's write-up and step 12's animation are render-time-bound |

*Where you stand:* per-thread `std::mt19937` seeded by thread index is done,
and `RT_THREADS` (6 October) makes thread-count sweeps possible from one
binary. Decomposition is still static scanline bands, which gives every thread
an equal number of *rows*, not an equal amount of *work*. Note that the image
currently depends on the thread count (RNG seeded by thread index), so the
"bit-identical to single-threaded" criterion needs seeding by pixel or tile
instead.

Part of "find out why" is already answered: 16 threads were measured at only
**2.05×**. The dominant cause was memory bandwidth — every ray copied the
3.3 MB background — and that is fixed. **Re-measure from scratch before drawing
conclusions.** What remains is band load imbalance, which is exactly what tiles
fix.

### Step 7 — Triangle meshes + glTF

| | |
|---|---|
| **Goal** | glTF 2.0 through **cgltf**; triangle intersection (Möller–Trumbore); vertex normals with interpolation; transform hierarchies flattened to world space |
| **Done when** | A real model renders correctly, and its frame time is recorded. (Step 8 took its baseline from the OBJ path instead, so nothing downstream waits on this.) |
| **Why it matters** | **It makes Blender the scene editor** |

*Where you stand:* OBJ meshes work; the triangle test is the textbook Cramer's
rule formulation, which already computes beta/gamma and throws them away — the
barycentrics interpolation needs, so half the work is done (step 9 piece 4
will start using them).
Missing: any glTF loader, `vn` parsing (meshes are flat-shaded today), and
flattening (the graph is walked per ray, transforming rays into local space
rather than geometry into world space).

Blender exports glTF with meshes, cameras and transforms, and lights through
the `KHR_lights_punctual` extension. Once the renderer reads it, "add a mesh"
means dragging one in within Blender instead of writing Lua.

**Format and loader, decided 6 October: glTF 2.0, read with cgltf.**

- **Why glTF over OBJ.** OBJ is geometry only: no hierarchy, cameras or
  lights, and its `.mtl` materials are an ad-hoc relic. glTF carries
  transforms, cameras, lights, PBR materials, UVs, normals **and tangents** —
  the last is what normal maps need, and OBJ cannot store it. It is the
  Khronos interchange standard (the group behind Vulkan), and Blender's
  exporter for it is excellent.
- **`.glb` for assets, `.gltf` for reading.** `.glb` is binary glTF: the JSON,
  the buffers and the images in one file. Keep `.glb` files in
  `assets/models/`, and add `*.glb binary` to `.gitattributes` beside `*.obj`.
  Export a `.gltf` (JSON plus a separate `.bin`) when you need to see what
  Blender actually wrote.
- **Why cgltf over tinygltf.** cgltf is one C header (MIT) that parses both
  `.gltf` and `.glb` and decodes no images, which suits a repo that already
  vendors lodepng. tinygltf is C++ and pulls in a JSON library and
  `stb_image`. cgltf goes into `third_party/` beside glm, lodepng and Lua.
- **Not before the deadline.** Step 9 is scored on OBJ on purpose; glTF is
  this step.

- **First milestone, before materials or lights:** export one cube and one
  camera from Blender, load them, render them. Check the camera by rendering
  the same framing in Blender. glTF is Y-up and Blender is Z-up, and the
  exporter converts between them by default, so an image that comes out
  rotated 90° is the conversion being applied twice or not at all.
- **Materials do not map one to one.** glTF carries PBR metallic-roughness;
  the renderer has Lambertian, Blinn-Phong, metal and dielectric. The loader
  needs a stated mapping (metallic → metal, transmission → dielectric,
  otherwise Lambertian from base colour), and that mapping is a decision worth
  writing down, not an import detail.
- **Lua stays for tests.** The furnace and every render test need exact,
  repeatable scenes in version control. glTF is for exploration scenes; see
  the open question at the end of §7.

### Step 10 — Emissive geometry + next event estimation

| | |
|---|---|
| **Goal** | Area lights, then explicit light sampling with shadow rays and area-to-solid-angle PDF conversion |
| **Done when** | A small bright light converges in a fraction of the samples it used to, with the same converged result. Graph both |

*Where you stand:* `Light` is a point-light struct — position, colour, and a
`falloff[3]` that is parsed from Lua and **never read by the shader**.

**Fixed 22 Sep, ahead of time: shadow rays used `MAX_T` as the far bound, so
geometry *behind* the light cast shadows.** It took a closed room to expose it.
The shadow ray's direction is `light->position - hit_point`, unnormalized, so
the light sits at `t = 1` and the bound has to be `1.0`; with `kMaxT` anything
past the light occludes it too. Outdoors there is rarely anything past a light,
which is why every existing scene rendered correctly and stayed byte-identical
after the fix. In `cornell_box.lua` the ceiling is always past a ceiling lamp,
so every surface shadowed itself and the frame came out black. The lesson is
not the one-line fix; it is that the scene set had no closed geometry in it
until then.

### Step 11 — Multiple importance sampling

| | |
|---|---|
| **Goal** | Combine BSDF and light sampling with the power heuristic |
| **Done when** | The furnace test still passes, and rough metal under a large area light shows no fireflies or dark bands |

*Where you stand:* depends entirely on steps 3 and 10.

### Authoring and viewing tools

Not a staircase step: none of these changes what the renderer computes. They
make scenes quicker to build and parameters quicker to try, and they solve two
separate problems:

- **Building scenes** — meshes, camera, lights. Blender already does this well.
- **Tweaking render settings and seeing the result** — spp, bounce depth,
  sampler choice. That needs a live window with controls, which Blender does
  not provide for this renderer.

Cheapest first:

0. **Blender → OBJ, today, no code.** The OBJ loader in `src/geometry/mesh.cc`
   reads positions, fans polygons into triangles and ignores `vt`/`vn`, so a
   Blender OBJ export drops straight into `gr.mesh`, the way `cornell_box.lua`
   places its three models. Camera, lights and materials stay in Lua. Tick
   *Triangulate faces* on export: the fan is only correct for convex polygons.
1. **Watch-and-re-render** (an hour or two). Re-run the renderer at low
   resolution and spp whenever a scene file is saved — a script around the
   CLI, no renderer change. Watch the whole `assets/scenes/` directory, not one
   file, because scenes pull in other files. Its usefulness scales with render
   time, which is one more reason step 8 came first.
2. **Blender → glTF as the scene editor** (a weekend). This is step 7.
3. **A Blender render-engine add-on** (bigger). A Python class registered
   through `bpy.types.RenderEngine` appears in Blender's render dropdown next
   to Cycles and Eevee. On F12 it exports the scene, runs this renderer on it
   and shows the result inside Blender, which is how external engines like
   LuxCore plug in. It depends on 2: the add-on hands over a glTF file. A
   strong portfolio piece for creator-tools roles, but only once 2 works.
4. **A GLFW + Dear ImGui viewer** (a weekend or two). A window that shows the
   image refining as samples accumulate, with sliders for the parameters.
   Useful for debugging samplers; it is not a mesh editor. The accumulation it
   needs already exists (`Framebuffer`, and the snapshots are this same idea
   written to disk). Build it as a **separate target**: §8 records the GL stack
   being removed so that the renderer and its tests build without it, and a
   viewer should not bring it back into either.

**Order:** 0 whenever it helps, 1 once re-running by hand gets tedious, 2 as
step 7, then 3. Do 4 once parameter-tweaking is frequent enough to justify it.

---

## 6. How measurements are taken

The rules every number in this file follows, each learned from a number that
was wrong without it:

| Rule | Why | Learned from |
|---|---|---|
| **Compare only same-day numbers**, or re-take the "before" alongside the "after" | The machine drifts: the same binary moved ~7% between 13 and 22 Sep | Step 8 before-numbers |
| **Interleave and rotate** configurations within each round | Fixed order biased the second run ~6% slow | Step 8 bench |
| **Prefer deterministic counts** to wall-clock where possible | A 9% change was invisible in a ±12% timing spread but exact in the counter | Step 8 shadow-ray guard |
| **Time with counting off; count in a separate run** | Counting cost 17–20% | `RT_STATS` |
| **Byte-identical output is the pass condition for refactors** | Any difference means the change did more than it claimed | Step 8 guard, step 5 aperture split |
| **Compare hashes only within one toolchain and one thread count** | GCC and MSVC differ on 0.154% of pixels; the RNG is seeded by thread index | Step 0, `RT_THREADS` |
| **Separate fixed per-frame cost from per-ray cost** before comparing at low spp | 42% of a 1 spp frame was a texture decode | Step 8 profile |
| **Expect the laptop to throttle; never compare early rounds with late ones** | Round times rose ~40% over the first ten rounds of a 30-round run, then levelled. Rotation spreads it evenly; the median resists it | Step 8 rung E |
| **Write the prediction down before measuring** | Several predictions here were wrong, and that was the finding | Steps 4, 5, 8 |
| **Record scene, resolution, spp and thread count** with every number | A rays/sec figure without them is not comparable to anything | Below |

### The original baselines (13 September, superseded)

Kept because their two corrections still stand. The current "before" table is
in step 8.

Release build, MinGW GCC, 20 hardware threads, 1 spp unless stated. **Render**
is the figure the renderer logs (`done in N ms`); **wall** is the whole
process. The gap was a fixed ~80 ms of start-up and decoding the 3.3 MB
background texture (since moved outside the timer and decoded once).

| Scene | Resolution | spp | Render | Wall |
|---|---|---|---|---|
| `assets/scenes/simple.lua` (5 spheres) | 256×256 | 1 | 16 ms | ~94 ms |
| `assets/scenes/macho-cows.lua` (17.4k triangles) | 256×256 | 1 | ~3490 ms | ~3580 ms |
| `assets/scenes/final_animation.lua`, one frame | 512×512 | 1 | — | ~230 ms |
| full 85-frame animation | 512×512 | 1 | — | ~20 s |

`macho-cows` over four runs: 3431, 3452, 3532, 3679 ms — 3490 ms, taken at
commit `3b5d72e` while `bvh not built (linear scan)` was still the only path.
`simple` over three: 20, 16, 16 ms.

**Two corrections came out of taking it:**

- **The scene is ~17.4k triangles, not ~35k.** Three cow instances share one
  5,804-face mesh (17,412), plus a 116-face buckyball, a 2-face floor, and six
  instanced arches of two `nh_box`es each, which `NonhierBox` expands to 12
  triangles apiece. `cow.obj`'s face lines are already triangles, so there is
  no quad split to double them. The old figure was roughly twice the truth.
- **The cow scene is ~218× slower than the sphere scene, not 33×.** The old
  ratio divided two *wall* clocks, and wall clock on `simple.lua` is 83%
  start-up — 94 ms of which only 16 ms is rendering. Comparing the render
  figures gives 3490 / 16. The fixed cost was diluting the very gap the number
  was meant to describe, and it flattered the linear scan by 6.6×.

**The gap it exposed, now closed:** the exit criterion asks for rays/sec, and
at the time nothing counted rays or linear-scan triangle tests — `bvh frame
totals` reported `0, 0`. Since 22 September `LinearScan` feeds the triangle
counter, and since 5 October the tree feeds both counters and rays are counted
by kind, all behind `RT_STATS=1`.

---

## 7. Off-staircase backlog

Cheap and worth folding in when you are next in the relevant file.

**Open:**

- [ ] **Light falloff** — `Light::falloff` is parsed and never read. Subsumed
      by step 10, but a two-line win before then.
- [ ] **Stratify the pixel jitter and the aperture disk.** Both are drawn
      independently at random, so `samples x lens_samples` is only a product
      and `lens_samples` means nothing on its own (step 5). Stratifying both
      2-D domains would cut variance at the same ray budget and give the two
      numbers separate meanings. The `TODO` sits at the jitter in
      `src/render/renderer.cc`; the out-of-focus noise in
      `renders/thin_lens_far.png` is what it would fix.
- [ ] **`Ray` getters are not `const`** — making them so (and the whole
      `IsHit` chain) removes the `const_cast` in `Sphere::IsHit`.
- [ ] **`Primitive::IsHit` returns `false`** instead of being pure virtual, so
      a primitive that forgets to override it silently renders nothing.
- [ ] **`NonhierBox` builds a 12-triangle mesh** per box. A box *is* an AABB —
      with step 8's slab test, boxes could get an analytic intersection free.
- [ ] **Two different `EPS`** — `kEpsilon` is `1e-6`, in
      `src/render/sampling.h`; `kEps` is `1e-5` in `src/geometry/mesh.cc`. Both
      are cross-referenced in the source.
- [ ] **`Image` stores 3 `double`s per pixel** (24 bytes; 25 MB at 1024²).
- [ ] **`JointNode`** is A3 vestigial — bound as `gr.joint`, no `IsHit`
      override, unused by any scene.
- [ ] **`RENDER_BOUNDING_VOLUMES`** is a compile-time define branched on at
      runtime in the hot path.

**Done:**

- [x] **`RenderBand`'s sample count was named three things** — `chunk`,
      `passes`, `pass_offset`, against `total_samples` and
      `g_samples_per_pixel`. Now `samples_to_add` / `samples_done`, and
      `start_row` / `end_row` for the rows (checked 6 October).
- [x] **Background filename** was hardcoded to `"kh_stain_glass.png"` in the
      renderer. Now a scene parameter, `gr.set_background` (step 3); only
      scene files name it (checked 6 October).

- [x] **Thread count** from `std::thread::hardware_concurrency()`, and since
      6 October overridable with `RT_THREADS`.
- [x] **`RayTracer` is a `Ray`** — renamed.
- [x] **`using namespace std/glm` in three headers** — removed, from the five
      `.cc` files that had one too. Names are qualified (`glm::vec3`,
      `std::vector`), as the Google style pass required. It also removed a live
      hazard: our `Reflect` and `glm::reflect` take the same argument types but
      use opposite sign conventions, and until the directive went, overload
      resolution decided which one a call got.
- [x] **`premake4.lua`** removed — the old build system.
- [x] **Scene clutter** — `sample.lua`, `nonhier.lua`, `simple-cows.lua` and
      `mucho-macho-cows.lua` deleted: A3 leftovers, unreferenced.
      `glass_spheres_camera_near/_far.lua` differed only in fov, and are now
      one `glass_spheres_camera.lua` that renders both framings — verified
      byte-identical to the two it replaced. 19 scenes to 14.
- [x] **The renderer translation unit** — renamed to `src/render/renderer.*`,
      and its public entry points (`Render`, `SetLens`, `SetSamplesPerPixel`,
      `SetSnapshotInterval`, `SetOutputPath`) lost their coursework prefix.

**Open question:** step 7 introduces glTF, which overlaps with what the Lua
scene layer does today. Decide then whether Lua stays as the scene/animation
driver with glTF only for geometry, or whether glTF takes over. The animation
pipeline currently depends on Lua. *Leaning, 24 September:* both, split by
job. Lua keeps the tests and the animation, because both need exact, versioned
scenes; glTF from Blender becomes the route for exploration scenes. Revisit
when step 7 starts.

**Deliberately kept:** `polyroots.cc` is 1079 lines of which only
`QuadraticRoots` is called, but its cubic and quartic solvers are most of the
work for torus and cone primitives.

---

## 8. Repo notes

- **Extracted from the CS488 coursework repo.** It lived in a subdirectory of
  `IbukunSanni/computer-graphics-portfolio`; `git subtree split` preserved all
  31 commits of its history.
- **Standalone build.** The renderer is CPU-only and never needed OpenGL — the
  old build linked it against the course framework (and so GLFW, ImGui and
  OpenGL) for a single 19-line header, `math_utils.h`. That header now lives
  here and the GL stack is gone entirely.
- **Vendored** under `third_party/`: glm, lodepng, Lua 5.3.1.
- **Sources are grouped under `src/`** by concern (core, math, geometry, scene,
  render, lua), with includes written relative to `src/`. Moved with `git mv`,
  so `git log --follow` still works.
- **Warning-free** under GCC (`-Wall`) and MSVC (`/W3`). Their renders agree to
  within 0.154% of pixels on `simple.lua`, not exactly — a byte-identical
  comparison is only valid within one toolchain.
- No top-level licence chosen yet — see the README's provenance note.
