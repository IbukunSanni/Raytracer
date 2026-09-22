# Raytracer — status and roadmap

The plan is a **staircase**: each step has an exit criterion you can point at
and say "done". Do not start the next step until the current one's criterion
passes.

Four steps are deferred past the 30 September 2026 deadline, so the sequence
skips numbers — the rule holds along the sequence, not along the numbering.
`ENGINEERING.md` owns that scope decision; section 3 below lays the steps out
in the order they are actually worked.

---

## 0. Working commands

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
./build/raytracer assets/scenes/final_animation.lua  # 85-frame animation
./build/raytracer assets/scenes/caustic.lua            # sun through a glass ball
```

Verification and tooling:

```bash
BVH_VERIFY=1 ./build/raytracer assets/scenes/hier.lua   # BVH vs linear scan, every ray
scripts/stitch_animation.sh renders/test_frames/bkeytest_frame_ 24 animation.mp4
```

Logging verbosity, via the environment rather than a rebuild — see the README
for the full grammar:

```bash
RT_LOG=off            ./build/raytracer assets/scenes/simple.lua
RT_LOG=debug          ./build/raytracer assets/scenes/simple.lua
RT_LOG=off,geom:debug ./build/raytracer assets/scenes/macho-cows.lua
RT_LOG_FILE=run.log   ./build/raytracer assets/scenes/simple.lua
```

Regression tests:

```bash
ctest --test-dir build --output-on-failure
```

Style, against the Google C++ Style Guide. `.clang-format` holds the
formatting rules and `.clang-tidy` the naming ones; the script runs both:

```bash
scripts/check_style.sh                    # report
scripts/check_style.sh --fix              # rewrite the formatting
cmake --build build --target style        # the same check via CMake
```

A pre-commit hook (`git config core.hooksPath .githooks`) catches formatting
on staged files, and `.github/workflows/style.yml` catches both on every push.
See the README's Style section for the two documented `NOLINT` exceptions.

Sources are listed explicitly in `CMakeLists.txt` rather than globbed, so a
new file must be added there — it will fail to link rather than silently not
build.

Source files are `lower_case.cc` / `lower_case.h`, and includes are written
relative to `src/` — `#include "geometry/mesh.h"`.

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
| `AABB::Hit()` | `src/geometry/aabb.h` | returns `true`, never culls | 8 |
| `BVH::Build()` | `src/geometry/bvh.cc` | leaves `built_` false | 8 |
| `BVH::Traverse()` | `src/geometry/bvh.cc` | only called once built | 8 |

---

## 2. Step 0 — unblock (before step 1)

Not part of the staircase proper, but step 1 changes the render loop and step 2
changes write-out, so fix these while that code is already open.

- [x] **Resolution is capped at 920×891 and segfaults above it.** Fixed:
      normalised UV mapping, cover-scaled and clamped. Renders at 2048×2048.
      Original description: The
      background center-crop in `RayTraceRgb` indexes outside the decoded PNG
      when the render is larger than the texture:
      `offsetWidthIdx = bgWidthMid - cropWidthMid` goes negative, unchecked.
      Verified: 512×512 fine, 920×920 and 1024×1024 both segfault. You cannot
      produce a high-resolution render until this is fixed, and the background
      is about to be replaced by an environment light in step 3 anyway.
- [x] **Portability: the code only built under GCC.** MSVC failed on the
      `and` / `or` alternative tokens and on `polyroots.cc` redefining `cbrt`
      (MSVC declares it dllimport, so redefinition is a hard error). Both now
      fixed; the tree builds warning-free under GCC and MSVC. Visual Studio's
      sampling profiler is therefore available for step 8's writeup.
      *(The two builds do NOT render identically. `Rng::Next` was made portable
      afterwards, taking the disagreement on `simple.lua` from 8.07% of pixels
      to 0.154%, but not to zero — see the README. Baseline and compare within
      one toolchain.)*
      *(Correction: `uint` was not an MSVC blocker as first diagnosed -- it was
      a project typedef in `image.h`, not a MinGW type. It has been removed
      anyway, since a project-wide `uint` collides with the POSIX one.)*
- [x] **Latent: children of a `GeometryNode` are transformed twice.** Fixed:
      the transform is applied once via shared `ToLocal`/`ToWorld`, and child
      traversal is factored into `HitChildren()`. Regression test added in
      `tests/`. Original description:
      `GeometryNode::IsHit` builds `local_ray`, then hands it to
      `SceneNode::IsHit`, which applies the same inverse again — and both
      restore on the way out. `SceneNode::IsHit` also overwrites the hit
      material with the geometry node's own, clobbering a child's. No current
      scene nests under a geometry node, so nothing is visibly wrong yet.

---

## 3. The staircase

> **Note on shape.** Step 3 is the pivot. Steps 1–2 are restructuring the
> renderer you have. From step 3 onward you are building a **path tracer** —
> `Sample`/`Eval`/`Pdf`, the furnace test, next event estimation and MIS are
> all path-tracing machinery, and the current `PhongMaterial` (ad-hoc
> `kd`/`ks`/shininess, not energy-conserving, no pdf) gets replaced rather than
> extended. Steps 4, 10 and 11 all depend on that interface existing. Worth
> knowing before you start, so it doesn't feel like scope creep when you get
> there.
>
> *(Done. It was substitutive, exactly as warned: `PhongMaterial` is gone, and
> so are the recursive `glm::mix` reflection and the ad-hoc ambient term.)*

> **Note on order.** The steps below are in **working order, not numeric
> order**, and keep their original numbers: `ENGINEERING.md`, `WALKTHROUGH.md`
> and a dozen cross-references in this file all name them, and renumbering
> would silently break every one. Scope and dates live in `ENGINEERING.md`;
> this file says what each step is and how it is scored.
>
> **In scope for 30 September 2026:** steps 4, 5, 8, 9, in that order, with
> step 12 as a stretch goal after them.
> Steps 6, 7, 10 and 11 are deferred and collected at the end. The staircase's
> "do not start N+1 until N passes" rule is broken here on purpose — it still
> holds within the in-scope sequence.

### Step 1 — Pixel jitter + accumulation buffer  ✅

Jitter the ray inside the pixel footprint. Keep a running radiance sum per
pixel and a sample count; divide at write-out. Restructure the render loop from
"loop N samples then output" to "accumulate forever, snapshot anytime".

**Done when:** edges are smooth, and you can dump an image at any sample count
without re-rendering. **— met.**

`src/render/framebuffer.{h,cc}` holds a per-pixel `dvec3` sum plus a sample
count; `Resolve()` divides into an `Image` and is `const`, so a snapshot never
disturbs the accumulation. A pass adds one jittered sample to every pixel.

- `samples` on the `gr.render` table — total samples per pixel. Was
  `gr.set_samples(n)`, retired once every render setting moved onto that table.
- `gr.set_snapshot_interval(n)` — writes `<out>_NNNNspp.png` as it goes

Verified: a single 64 spp render emitted 16 images; RMS against the final
result fell monotonically 0.820 → 0.083 (4 → 60 spp); the sphere silhouette
goes from hard stair-steps at 1 spp to a smooth gradient at 64.

Two notes for later:

- Jitter is **uniform**, not stratified. Stratified samples converge faster
  for the same count — a cheap upgrade once there is a reason to care.
- The sample count is global, not per pixel. Adaptive sampling (backlog) will
  need per-pixel counts.
- Thread bands own disjoint rows, so `Add()` needs no atomics. Step 6 keeps
  tiles disjoint too, but that assumption is worth rechecking then.

Thread count now comes from `hardware_concurrency()` rather than a hardcoded
16, which is why timings improved slightly on a 20-core machine.

### Step 2 — Linear color pipeline  ✅

Radiance stays linear internally. sRGB transfer applied only at write-out. Tone
mapping as a separate, swappable stage (Reinhard now).

**Done when:** you can disable tone mapping and see a raw linear dump, and a
0.5 albedo surface under a 1.0 light reads as 0.5 in linear, not 0.73. **— met.**

`core/tone_map.{h,cc}` holds both stages: `Apply()` (`None` / `Reinhard` /
`ReinhardExtended`; `ACES` is a stub) and `EncodeSrgb` / `DecodeSrgb`.
`Framebuffer::resolve` stays linear; `Image::SavePng` is the only place bytes
are made. The background PNG is `DecodeSrgb`'d on input, replacing the old flat
`0.3` scale.

- `gr.set_tonemap{ operator=, exposure=, white_point=, srgb= }` — all optional;
  `srgb = false` is the raw linear dump.
- `tests/scenes/tonemap_probe.lua` checks both dumps stay consistent.

At the time the probe read ~0.375, not 0.5: the always-on reflection `glm::mix` blended in
25% black. Step 4's BSDF fixes that.

### Step 3 — BSDF interface + furnace test  ✅

Define the interface before you have many materials:
`Sample(wo, rng) -> {wi, throughput, pdf}`, `Eval(wo, wi)`, `Pdf(wo, wi)`.
Port your existing diffuse to it. Then build the furnace test: uniform emissive
environment of radiance 1, albedo-1 diffuse sphere.

**Done when:** the sphere is invisible against the background. If it's darker,
you're losing energy; brighter, you're double-counting. **— met.**

`Material` is now a pure BSDF interface (`Eval` / `Pdf` / `Sample`), implemented
by `LambertianMaterial` and `BlinnPhongMaterial` — the latter a normalised
`(n+2)/8π` lobe with luminance-weighted two-lobe sampling, so `gr.material` is
energy-conserving for `kd + ks ≤ 1`. `RayTraceRgb` is an iterative throughput
walk with Russian roulette; the recursive `glm::mix` reflection, the ad-hoc
ambient term and the whole Blinn-Phong inline block are gone.

Both materials are reachable from Lua as `gr.lambertian{ kd = ... }` and
`gr.blinn_phong{ kd = ..., ks = ..., shininess = ... }`, with `gr.material`
kept as a deprecated positional alias so existing scenes load unchanged. Scene
load now warns when `kd + ks > 1` per channel — the precondition every energy
check runs under. That immediately flagged eleven of the twelve shipped
scenes, `simple.lua` worst at 1.7 in green. Harmless under Whitted shading,
where light never bounced twice; under a path tracer it compounds every
bounce.

Verified at two levels:

- **`tests/bsdf_test.cc`** (its own CMake target) integrates the BSDFs
  directly, with tolerances at 4 standard errors computed by Welford from the
  run itself. White-furnace ρ = 1, `E[cosθ] = 2/3` for the cosine sampler, and
  energy conservation across five `kd`/`ks`/exponent cases at three angles of
  incidence. Two of the checks tie `Sample()` to `Pdf()` without the
  cancellation trap — the obvious `f·cos/p` estimator is degenerate for a
  Lambertian and returns the albedo even if the sampler is broken. Deleting the
  half-vector Jacobian fails the sampler checks, so the tests bite.
- **`tests/scenes/furnace.lua`** is the criterion above: uniform 255 at
  radiance 1, uniform 128 at radiance 0.5. The second render exists because
  255 clips, which would hide a too-bright result. `FURNACE_MATERIAL` picks
  the material, so the same scene is the criterion for the diffuse, the
  mirror and the metal.

Three things worth carrying forward:

- The diffuse term went from `kd·N·L` to `(kd/π)·N·L`, so every scene's lights
  were scaled by π. Specular does not scale the same way — it gained the
  `(n+2)/8π` normalisation — so highlights are stronger than before. No single
  factor fixes both; that is what an energy-conserving lobe does.
- The background is now a lat-long environment map sampled by ray direction
  (`gr.set_background`), so it lights the scene rather than just backing it.
  Empty path means `ambient` is a uniform environment. The old screen-space
  lookup could not work for bounce rays, which have no pixel.
- Blinn-Phong loses energy at grazing angles: pure specular `ks = 0.9, n = 50`
  measures ρ = 0.835 at 0° but 0.056 at 80°. Expected — no Fresnel, no
  multiple scattering between microfacets — but it is why grazing highlights
  render dim. And 15% of specular samples at 80° scatter below the horizon and
  are discarded, which MIS would recover.

### Step 4 — Refraction and reflection  ✅

Dielectrics (Snell, TIR, Schlick), smooth metal, rough metal. All through the
interface from step 3.

**Done when:** a glass sphere shows caustics and correct TIR at grazing angles,
and an albedo-1 dielectric is invisible in the furnace. The second clause is
necessary but catches only energy errors — it cannot see a wrong direction at
all, which is most of what is left here. See the retro on the transmit-only
rung.

*Where you stand:* the dielectric reflects and refracts, splits between the
two by Fresnel, reflects totally past the critical angle, and scales the
transmitted weight by the relative η² — every rung below is ticked. Adding a material is a new `Material` subclass plus a `gr.*`
constructor and one row in `grlib_functions`; `push_material` is the shared
tail and `set_material` never learns the concrete type.

`Refract()` satisfies Snell at every angle it is now asked about, `Sample()`
stops asking past the critical angle, and the branch it takes is drawn with
Fresnel's probability. All three are held by `tests/bsdf_test.cc`, suite
`bsdf/dielectric`: fifteen crossings of glass in air and air in water,
entering and exiting, on both sides of each critical angle, plus a measured
reflected fraction against independently computed reflectance.

**Step closed 13 September 2026.** Every direction the material returns is the
right direction, drawn with the right probability, and the transmitted branch
carries the relative η² with it. The last criterion was the picture — caustics —
and it is met and measured: see the end of this section. The one thing that
judgement turned up is that *which light source you pick decides whether a
caustic is possible at all*, and two of the three available here cannot carry
one.

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
"infinite here, zero everywhere else". The convention, settled by the two rungs
below: `IsSpecular()` marks the material, `Eval()` and `Pdf()` return 0, and
`Sample()` returns `pdf = 1` with the whole weight in `brdf`, which the renderer
applies unmodified. Returning 0 is correct rather than a cop-out — next event
estimation can never land on a delta lobe, for the same zero-measure reason BSDF
sampling can never hit a point light.

**Climb this in rungs.** Each one builds, renders, and has its own pass/fail
signal — do not write the finished dielectric in one go.

- [x] **Perfect mirror.** No refraction at all: `Sample()` reflects `view_dir` about
      `normal` and returns `pdf = 1` with `brdf = albedo`. This rung exists to
      force the delta-pdf plumbing while nothing else is moving. *Signal:* an
      albedo-1 mirror sphere in the uniform furnace must be **invisible**, since
      it reflects radiance 1 from every direction. **— met.**

      `IsSpecular()` joins the `Material` interface, defaulting to `false`, and
      `renderer.cc` branches on it to multiply `brdf` straight into the
      throughput. The general `brdf * cos / pdf` estimator would divide out a
      cosine and multiply the same one back; that round trip is not exact in
      float, and the drift left the mirror one code darker than its
      environment. Lua: `gr.mirror{ albedo = {...} }`.

      `MeasureSampler()` assumes a density, so it now skips a specular material
      rather than false-failing on it, and `CheckDeltaContract()` asserts the
      real invariant: `pdf == 1` and `brdf == albedo` exactly, on every draw.
      `tests/scenes/furnace.lua` with `FURNACE_MATERIAL=mirror` renders at
      radiance 1 and 0.5, both perfectly uniform (`min == max == 255` and `128`).
- [x] **Fuzzy reflection (rough metal).** A specular lobe centred on the
      mirror direction, widened by a roughness/fuzz parameter. Placed here,
      right after the mirror, because it needs none of the dielectric
      machinery below: no Fresnel split, no Snell transmission, no TIR —
      just the reflection half, blurred. **— met.**

      `MetalMaterial` draws `normalize(reflect(in, normal) + fuzz * u)` for a
      uniformly random unit `u`. Displacing a unit vector by `fuzz` and
      renormalising sweeps a cone of half-angle `asin(fuzz)` — the tangent
      from the origin to the offset ball bounds the lean — which is what
      makes `fuzz = 1` the widest lobe and why the constructor clamps to
      `[0, 1]`. Reachable from Lua as `gr.metal{ albedo = {...}, fuzz = f }`.

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
      nudges the bounce origin to `hit_point + N * kEpsilon`, always outward,
      which puts a transmitted ray on the wrong side of its own surface and
      lets it immediately self-hit. It must follow the scattered direction,
      not the normal. That is the only renderer-side change: the
      `dot(normal, out) <= 0` guard lives in the two diffuse `Eval`s, which a
      delta dielectric never calls, and the `pdf <= 0` break passes a `pdf` of
      1 through untouched. **— met.** `assets/scenes/glass_spheres.lua` reaches
      it, and the offset is now scale-relative as well as sign-aware, for a
      reason that had nothing to do with dielectrics.

      **The predicted signal was met, and the reasoning behind it was still
      wrong.** It read as though `index = 1.0` were the weak case and a higher
      index would test more. It is not, and it does not: at `ior = 1.5`, with
      TIR demonstrably broken, the furnace still renders 65536 of 65536 pixels
      at exactly 128. A dielectric's throughput is identically 1 and the
      environment is uniform, so *every* scattered direction returns the same
      radiance. The furnace is an energy test and it is structurally blind to
      direction errors for this material — at any index. Raising it would buy
      false comfort, which is why `furnace.lua` keeps 1.0 and says so.

      The consequence is a split, not a blanket failure. The furnace still
      scores *energy* errors on the rungs below, but not uniformly: a missing
      η² factor cancels over any path that both enters and leaves the glass,
      and the rung below measured that it does not surface on the paths that
      die inside either. What the furnace can never score is a *direction*
      error, and TIR and Snell are exactly that. Those need a signal it cannot
      give.
- [x] **A direct test for `Refract()`.** First, because it is what scores the
      rungs after it. Sweep incidence angles entering and exiting, and assert
      unit length, Snell below the critical angle, and the correct hemisphere
      above it. It is a dozen lines, it needs no renderer, and it is the only
      check that fails today. *Signal:* it fails on the current TIR handling
      before it passes on the fixed one — the same standard the half-vector
      Jacobian was held to in step 3. **— met**, and it found more than it was
      written for.

      **The predicted failure and the actual failure were different failures.**
      The plan named unit length as the tell, which is true of `Refract()` and
      useless through `Sample()` — `Sample()` calls `glm::normalize` on the
      result, so the long tangent vector reaches the caller as a perfectly
      unit direction pointing the wrong way. Length is now asserted on the
      helper directly, for the crossings where Snell has a solution; through
      the material, the assertions that bite are hemisphere and Snell's angle.
      A test written one layer too high would have passed on the broken code.

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
      way. **— met.** `Sample()` now decides before calling, and `Refract()`
      is documented as having no answer above the critical angle rather than
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
- [x] **Fresnel split.** Add Schlick and choose between the two branches with
      probability `R(θ)` rather than always transmitting. Both branches
      already exist and are tested, so this rung is the choice and nothing
      else — **do not absorb the remainder.** An earlier draft of this rung had
      the non-reflected fraction go to black, which made sense when
      transmission did not exist yet; doing it now would delete working
      physics to stage a picture that is deliberately wrong. It is also what
      finally uses `Sample()`'s `rng`, unused since the material was written.
      **— met**, with one instruction overturned.

      **Schlick was the wrong primitive, and this rung named it in the title.**
      It is accurate for a typical interface at moderate angles and wrong at
      both ends of the range this material actually spans. At `index_ratio`
      1 there is no interface and R must be 0 everywhere; Schlick returns
      `r0 + (1 - r0)(1 - cosθ)⁵`, which climbs to 1 at grazing whatever `r0`
      is, so it reflects 92% of rays at 89° off a surface that is not there.
      Approaching the critical angle from inside it stays flat near 0.04 and
      then hands over to a TIR branch at 1.0.

      The exact dielectric Fresnel equations fix both and cost a `sqrt` and
      two divides on a lobe evaluated once per bounce. They also delete the
      `cos θt` correction this rung used to prescribe: the exact form takes
      the incident cosine and handles either side itself, so the special case
      stopped existing rather than getting implemented. Leaving `brdf` at 1 on
      both branches stays right — choosing with probability R and weighting
      R/R cancels exactly.

      **The furnace did catch it, and the reason is worth keeping.** This rung
      predicted it could not: both branches carry throughput 1, so any mixture
      returns radiance 1 in a uniform environment. That reasoning holds, and
      the furnace still cannot score the *ratio*. What it caught was longer
      paths — Schlick reflecting ~0.92 at grazing sends rays rattling inside
      the sphere until they exhaust `kMaxDepth` and return black. The furnace
      failed **dark**, `lo = 233` against an expected 254. Roulette never got
      near them, because throughput never drops. A test can be blind to a
      quantity and still see what that quantity does to path length.

      **One test had been passing by luck.** The TIR case asserted
      `reflected == expected` from a single `DrawOnce` at a fixed seed. Once
      the branch became probabilistic that assertion was a coin flip, and it
      passed only because `Rng(85)`'s first draw happened to exceed R in all
      fifteen crossings. Three cases in the suite are now statistical, over
      200k draws each, and a sixth measures the split itself against
      reflectances computed outside the renderer — comparing `Reflectance()`
      to itself would agree however wrong it was. That case rejects Schlick on
      five rows, worst 0.387.

      *Signal:* **not the furnace.** Both branches carry throughput 1, so any
      mixture of them returns radiance 1 in a uniform environment — the same
      blindness the transmit-only rung ran into, for the same reason. Count
      draws instead: the reflected fraction over many samples must match
      `R(θ)`, near 0.04 at normal incidence for `ior = 1.5` and climbing to 1
      at grazing.

      **Schlick takes the cosine on the thinner side, and that is not always
      the incident one.** The approximation is derived for a ray entering the
      denser medium; fed the incident cosine on the way *out* of glass it
      returns ≈0.04 right up to the critical angle and then hands over to a
      TIR branch that reflects everything — a step from 0.04 to 1 with nothing
      in between, which renders as a hard ring. Evaluate it on `cos θt` when
      `index_ratio > 1` and it climbs to exactly 1 as `θt → 90°`, meeting the
      TIR branch continuously because that limit *is* the critical angle. The
      ring is the signal: if the rim has an edge, the wrong cosine went in.
      Leaving glass at η = 1.5, critical angle 41.81°:

      thetaI   Schlick(cos_i)   Schlick(cos_t)   exact Fresnel
        0.0       0.0400           0.0400          0.0400
       35.0       0.0402           0.0672          0.0861
       40.0       0.0407           0.2456          0.2453
       41.8       0.0410           0.9075          0.8908

      The middle column is the one that meets the TIR branch. The left column
      is flat across the whole approach and then jumps.
- [x] **η² radiance scaling across the interface.** Radiance is not invariant
      through refraction; it scales by the relative η². `brdf` was 1 on both
      branches, which is right for reflection and wrong for transmission.
      **— met.** The transmitted branch now carries `index_ratio²`, and
      `tests/bsdf_test.cc` sweeps it over every crossing in `kCrossings`.

      **The exponent came from the renderer, not from the physics.** The
      statement "radiance scales by η²" is about light travelling into the
      denser medium. `RayTraceRgb` starts at the camera and carries a
      throughput forward along the reverse path, so what it transports is
      importance, which scales as radiance's reciprocal — Veach's
      non-symmetry of refraction. Entering therefore *darkens* by 1/η² and
      leaving brightens by η², which is the opposite of what this rung
      predicted when it was written, and the prediction was the physics
      statement applied to the wrong quantity.

      **The furnace prediction was wrong, and the reason is sharper than a
      near miss.** This rung expected the paths that die inside the glass to
      leave the entering factor uncancelled and come back bright. They cannot:
      a path that dies contributes nothing at all, so there is nothing for an
      uncancelled factor to scale. Every path that *does* reach the
      environment has crossed out as many times as it crossed in, so the mean
      is unchanged and the furnace is blind to this rung at any index — not
      narrowly, but exactly.

      **It is blind to the sign for a second and stronger reason.** The
      cancellation argument says a wrong exponent still returns 1 per path.
      Measurement says more: with the sign flipped, the glass furnace is
      **byte-identical** to no η² factor at all. Russian roulette uses
      `q = min(0.95, max(throughput))`, so a throughput of 2.25 inside the
      glass and a throughput of 1 both clamp to 0.95 — same draws, same
      stream, same image. The furnace never sees the factor, rather than
      seeing it and cancelling it. At `ior = 1.5`, 64 spp, radiance 0.5:

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
      on the glass. If this rung had been scored on a picture it would have
      been called a no-op.

      **Square the index, not the ratio.** `1/(η·η)` times `η·η` is exactly 1
      in float at 1.5 and at 1.05, where `(1/η)·(1/η)` times `η·η` is not;
      it is never worse. It is also not universal — diamond at 2.417 lands one
      ulp short either way — so the round-trip test records that rather than
      claiming exactness for every index. This is the same concern that left
      the mirror one code darker than its environment in the first rung.

      **A test was passing by luck again, in the same way as last rung.** The
      delta-contract case asserted `brdf == 1` on a single seeded draw
      entering glass. Once transmission stopped weighing 1, that assertion was
      correct only when the seed happened to reflect. It now uses the two
      crossings that weigh 1 whichever branch they draw: index 1, which is not
      an interface, and a crossing past the critical angle, where only the
      reflected branch exists.

**Caustics — the last criterion, and it is met.** `assets/scenes/caustic.lua`:
a glass ball of radius 0.5 at `ior = 1.5`, floating 0.3 above a diffuse floor,
lit by a 4°-radius sun in a lat-long environment map. A ball lens focuses at
`nR / (2(n-1))` = 0.75 from its centre, which lands the focus at `y = -0.45`,
just above the floor at `-0.5` — that is what makes the spot a point instead of
a smear. Across the focal row at 512 spp:

    open floor        74.7
    shadow annulus    60.9     82% of floor
    caustic core     147.7    1.98x floor, 2.43x annulus

Dark ring, bright core, measured rather than eyeballed. `docs/images/step4-caustic.png`.

**Getting a caustic at all took choosing the light source, and two of the three
options cannot carry one.** This is the part worth writing up:

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

**The standing warning about Russian roulette was wrong, and the measurement is
cleaner than a near miss.** This section used to say roulette was most likely to
kill caustic paths and to raise `kRrStartDepth` before doubting the BSDF.
Rebuilt with `kRrStartDepth` at 8 instead of 3, the same render gives
core/floor of **1.98× either way**, floor noise of **12.2% either way**, and the
same frame time to within noise. Roulette kills paths by *low throughput* —
`q = min(0.95, max(throughput))` — and a caustic path is a bright one: floor
albedo 0.75, dielectric branches weighing about 1, so `q` never falls near
zero. The advice named the wrong mechanism.

What the noise actually is: the sun subtends 0.0153 sr, so a cosine-weighted
diffuse bounce finds it about **0.49%** of the time, and BSDF sampling is the
only way to find it — next event estimation is step 10, deferred. That is why
512 spp still grains. It is a sampling-strategy limit, not a BSDF error, and
step 10 is what fixes it.

One thing left over, not blocking:

- `assets/scenes/final_animation.lua:33` and `:37` call `gr.material` with **six**
  arguments — the trailing `0.0, 0.0, 1.0` and `0.4, 0.0, 1.0` look like
  reflectivity, transparency and IOR. Lua silently discards them and always
  has, so those two lines are lies in the scene file. Delete them, or make them
  real, when the dielectric constructor lands. The table-argument constructors
  (`gr.blinn_phong{...}`) would have rejected an unknown field; the deprecated
  positional `gr.material` still will not.

### Step 5 — Thin-lens camera  ✅

Sample a point on the aperture disk, aim through the focal plane. Expose the
lens through the scene language.

**Done when:** you can rack focus between a near and far sphere.

**Done 22 Sep, measured.** `assets/scenes/thin_lens.lua` renders three spheres
at three depths from one eye — once through a pinhole, once focused near, once
focused far — at a matched 256 rays/pixel so blur is the only variable. Mean
gradient magnitude in a +/-4 px ring on each sphere's silhouette:

    shot                near ball   mid ball   far ball
    pinhole                  8.70       8.08      17.13
    near  (focus 2.05)       8.59       4.88       6.54
    far   (focus 7.01)       2.78       3.76      17.22

`docs/images/step5-rack-focus-pinhole.png`, `-near.png`, `-far.png`.

The focused sphere holds **0.99x** and **1.01x** of its pinhole sharpness while
the others fall to 0.32-0.60x. An in-focus edge is exactly as sharp as a
pinhole edge — the aperture can only cost sharpness away from the focal plane,
which is the invariant to check first if this ever regresses.

**The camera basis was mirrored and the pixel grid was off by half a pixel.**
Found while writing the lens, because the lens is the first code to consume
`u_vec`/`v_vec` as directions rather than as step vectors. `u = cross(up, view)`
is screen *left* and `v = cross(u, w)` is screen *down*; `RenderBand` cancelled
both by stepping `(w - x)` and `+y` from a top-right corner, so the image came
out upright and the mirroring stayed invisible. The basis is now
`u = cross(w, up)` (right) and `v = cross(u, w)` (up), with the orientation
signs written at the point of use: `+(x + 0.5)*u` and `-(y + 0.5)*v` from a
top-left corner.

The half-pixel was the real bug. Stepping by `x` and `y` with no `+0.5` samples
pixel *corners*, so the frame sat half a pixel off in both axes and the sample
window overhung the top-left edge. Measured on a scene built mirror-symmetric
about both screen axes, where correct registration must render symmetrically:

    spp     L-R asymmetry      T-B asymmetry
    400     0.622  before      0.623  before
    400     0.219  after       0.217  after
    6400    0.592  before      0.593  before
    6400    0.054  after       0.053  after

16x the samples barely moves the old numbers — a systematic bias does not
average away — while the new ones fall by almost exactly 4x (= 1/sqrt(16)),
which is Monte Carlo noise converging to zero. Also fixed in the same pass:
`d_float` computed `h / 2` in `size_t`, truncating for odd heights (225 to 112).

**`samples` and `lens_samples` are one budget, not two.** `total_samples` is
their product and nothing downstream sees the factors, because `RenderBand` is
a single flat loop: each iteration jitters the pixel *and* draws one aperture
point, so every ray is both an AA sample and a lens sample. 16x16, 256x1 and
1x256 render **byte-identical**. That is the correct Monte Carlo structure —
256 independent samples of a 4-D space — but the API implies a decomposition
the renderer does not have. `lens_samples` is a cosmetic multiplier today;
stratifying both domains is what would make it real, and is in the backlog.

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

**The focal plane is a plane because of a dot product, and here is the
proof.** `focus_t` divides by `dot(pin_dir, w_vec)`, the axial component, not
by `length(pin_dir)`. Dividing by the length places each focal point a fixed
distance along *its own ray*, so the locus is a sphere around the eye rather
than a plane.

Seven identical spheres, all at the same axial distance, spread across the
frame at 800x450 and 40 deg fov, aperture 0.70, focused on exactly their
distance. A flat focal surface puts every one of them in focus; a curved one
cannot. Silhouette sharpness, and the ratio between the two builds:

    sphere      x=-5.4  x=-3.6  x=-1.8  x=0.0  x=+1.8  x=+3.6  x=+5.4
    dot (flat)   19.24   25.86   27.10  26.90   27.12   25.84   19.25
    length       13.16   23.35   26.93  26.88   26.89   23.31   13.29
    ratio         0.68    0.90    0.99   1.00    0.99    0.90    0.69

`docs/images/step5-focal-plane-flat.png` and `-curved.png`. The ratio row is
the statistic that matters — the absolute numbers fall off at the edges in
*both* builds, because an off-axis sphere projects to an ellipse and the
measurement ring is a circle. The centre pixel is **1.00**: the two divisors
agree exactly on axis, which is why a rack focus on a centred subject cannot
detect this error at all. It only shows up off-axis, where nobody is looking.

The curved build was produced by editing the one divisor, rendering, and
reverting — the same technique as the half-pixel measurement above, and worth
repeating whenever a bug is invisible in the shipped build.

**`SampleUnitDisk` has two consumers, and that is a trap.** Step 3 made it the
body of cosine-weighted hemisphere sampling as well, via Malley's method — a
uniform disk sample lifted to the hemisphere is cosine-distributed, which is
why one function serves the aperture and the BSDF. So a *shaped* aperture
(hexagonal bokeh, a bladed iris) is correct for the lens and would silently
break every BSDF's `Pdf`/`Sample` agreement, because the pdf still assumes a
uniform disk. The furnace test would catch it — that is what
`pdf mass == frac above horizon` is for — but only if you run it. Give the lens
its own sampler before shaping the aperture, rather than after.

**What the model assumes, and what it therefore cannot do.** A thin lens is
an idealisation: the aperture has area but no thickness, no glass and no
aberration. Three consequences worth stating rather than discovering:

- **The disk is sampled uniformly**, so the bokeh is a uniform disc. Real
  optics vignette — the aperture a corner pixel sees is a lens-shaped sliver,
  not a circle — and there is no `cos^4` falloff here either. Both would be
  additions to `ThinLensRay`, not corrections to it.
- **Nothing is chromatic.** One focal point serves all three channels, so
  there is no longitudinal or lateral colour fringing at any aperture.
- **The ray direction stays unnormalized, and its length now depends on the
  camera.** A pinhole ray has `|dir| = d_float` (about 420 at 400x225, 30 deg
  fov); a lens ray has `|dir| = focus_distance` (about 4 in the same scene).
  Since `kEpsilon` and `kMaxT` are expressed in units of `|dir|`, turning the
  lens on shifts the effective near clip by ~100x. It is harmless today
  — `kMaxT` is `FLT_MAX` and the epsilon stays sub-micron on a primary ray
  that starts in empty space — but **step 8's slab test must not normalize**,
  and any future epsilon tuning has to hold for both cases.

**Known artifact, not a bug.** The far shot's out-of-focus near sphere is
visibly blotchy. Its circle of confusion is ~36 px, so a single pixel's rays
genuinely disagree — some hit the sphere, some miss it entirely — and variance
in an estimated mean is what noise is. An in-focus pixel's rays all strike
nearly the same point and agree. Same 256-ray budget, a far harder integral.

### Step 8 — BVH

SAH construction, flattened to a linear array, iterative traversal.

**Done when:** you can state rays/sec before and after on the same scene, and
explain where the remaining time goes. This is your first serious profiling
writeup.

**The before-numbers are taken. 22 September 2026, this machine, on the
guarded build** -- the specular shadow-ray guard below is part of the
baseline, so that the tree is measured against it rather than credited with
it. Do not compare against the 13 September figure of 3490 ms: the same
uninstrumented binary measures ~3960 ms today, so the machine, not the code,
moved by about 7%. A comparison is only valid against numbers taken on the
same day as the after-numbers, or re-taken alongside them.

| Scene | Res | spp | pixel-samples | Triangles | Render | Triangle tests |
|---|---|---|---|---|---|---|
| `simple.lua` (5 spheres, no mesh) | 256x256 | 1 | 65,536 | 0 | 17 ms | 0 |
| `macho-cows.lua` | 256x256 | 1 | 65,536 | 17,530 | **4067 ms** | **3,264,652,910** |
| `rtiow_final.lua` (balls and boxes) | 480x270 | 1 | 129,600 | 9,064 | **6087 ms** | **3,461,967,608** |
| `cornell_box.lua` (car, drone, ship) | 400x400 | 32 | 5,120,000 | 21,084 | **940,119 ms** | **685,905,974,124** |

Means of three: `simple` 18/17/17, `macho-cows` 3896/4058/4248, `rtiow_final`
6064/5970/6228. The Cornell box is a single run, at 15m40s.

One row is dropped rather than re-taken: `keyblade.obj` was a probe with no
scene file behind it (256x256, 1 spp, 43,354 triangles, 17,062 ms,
15,917,334,392 tests). It is not reproducible from the repository, so it is
not a baseline — only the two committed scenes are.

**The derived quantities, which are what a comparison actually needs.**
Wall-clock alone cannot separate "the tree is working" from "the machine was
busy"; these can.

| Scene | tests / pixel-sample | scans / pixel-sample | M tests/s | ms / pixel-sample |
|---|---|---|---|---|
| `macho-cows` | 49,815 | 2.84 | 803 | 0.0621 |
| `rtiow_final` | 26,713 | 2.95 | 569 | 0.0470 |
| `cornell_box` | 133,966 | **6.35** | 730 | 0.1836 |

Read left to right:

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
  fall out of cache. Extrapolating a time from *another* scene's rate is
  worth about 30 per cent; extrapolating within one scene is good to about
  1 per cent.
- **ms / pixel-sample** is the figure to scale a render time by. Doubling spp
  doubles it, and that held on the pre-guard build: the Cornell box took
  532,435 ms at 16 spp and 1,069,428 ms at 32, a factor of 2.009.

**The two comparison scenes, and why both stay.** `rtiow_final` and
`cornell_box` are deliberately opposite, and the tree should move them by
different amounts:

- `rtiow_final` is **open**, and most of its primitive count is 238 separate
  12-triangle box meshes. A per-`Mesh` tree gives each of those its own
  trivial tree and does nothing for the linear walk over 460-odd scene nodes.
  Expect a modest gain, dominated by the ship's 6,208 triangles.
- `cornell_box` is **closed**, and its 21,084 triangles sit in three meshes a
  ray bounces between until `max_depth` stops it. Almost all of the cost is
  inside meshes, which is exactly what the tree indexes. Expect the large
  gain here.

Per pixel-sample the Cornell box costs **3.9x** what `rtiow_final` does
(0.1836 ms against 0.0470). That ratio, not the raw wall-clock, is the honest
comparison — the two run at different resolutions and sample counts.

**`cornell_box.lua` needs the shadow-ray fix to render at all**, which is why
that fix is committed ahead of this step: with the far bound at `kMaxT` the
ceiling occludes the lamp for every surface and the frame comes out black.
See the step 10 note.

**The specular shadow-ray guard, and why it landed before the tree.** Next
event estimation ran at *every* hit, including hits on mirror, metal and
dielectric. Those materials answer `Eval` with exactly zero for every
direction but their one, and a point light is never on it %s so each such hit
bought a full occlusion traversal and multiplied the result by zero. The NEE
loop is now guarded by `!material->IsSpecular()`.

Both binaries were built and run alternately, one after the other, so that
any drift in the machine landed on both:

| Scene | Triangle tests, unguarded | guarded | Removed |
|---|---|---|---|
| `macho-cows` | 3,264,652,910 | 3,264,652,910 | **0%** |
| `rtiow_final` | 3,904,109,528 | 3,461,967,608 | **11.3%** |
| `cornell_box` | 753,885,512,940 | 685,905,974,124 | **9.0%** |

`macho-cows` does not move at all, because it has no specular material in
it; that zero is the control. The two scenes that do move are the two that
matter, and the Cornell box moves least of the pair despite being the most
specular scene here %s its metals are enclosed, so a ray that skips a shadow
test still goes on to bounce.

**The output is byte-identical, and that is the pass condition, not a
footnote.** `Eval` returns exactly `vec3(0.0f)` and the loop draws no random
numbers, so neither the arithmetic nor the RNG stream shifts. Verified at
both ends of the range: the 200x200 4-spp probe and the full 400x400 32-spp
render both hash the same before and after. Any difference at all would have
meant the guard was throwing away real light rather than a zero.

Two things this settles about method. **Wall-clock could not have found
this.** Across three interleaved probe runs each way the means were 29,647
and 28,986 ms %s 2%% apart, inside a +/-12%% spread. The triangle counter
returned the identical figure to the digit on all three runs. When a change
is a 9%% one, only the deterministic instrument can see it. **And it had to
go in before the baseline, not after.** Had the guard landed after the tree,
the tree would have collected credit for the 9%% as well, and there would be
no way left to separate them.

**The triangle counter had to be added before the tree, not after.**
`g_triangles_tested` was only incremented inside `BVH::Traverse`, so on the
linear-scan path it read zero and the *data* half of the comparison did not
exist. `BVH::CountTrianglesTested()` now takes one atomic add per scan with
the whole face count — identical to a per-triangle count, since the scan
tests every face unconditionally, and one contended cacheline instead of
thousands. Measured against the uninstrumented build it costs nothing above
noise; if anything the instrumented build ran faster, which is how you know
the difference is the machine.

Reproduce any row with:

```bash
RT_LOG=info,geom:debug ./build/raytracer assets/scenes/macho-cows.lua
```

Quality renders, kept as the visual before-state:
`docs/images/step8-baseline-rtiow.png` (480x270, 192 spp, **21m31s**
unguarded) and `docs/images/step8-baseline-cornell.png` (400x400, 32 spp,
**17m49s** unguarded, **15m40s** guarded). Neither image was re-rendered,
because neither changed: the guarded build reproduces both bit for bit.

Both make the case for this step, from opposite directions: the first has
6,208 ship triangles with no bounding-volume early-out, so every ray that
reaches the sky pays for all of them; the second has no sky to reach.

`assets/scenes/macho-cows.lua` remains the primary comparison scene: it is
the one with a published history.

*Where you stand:* scaffolded. `BVHNode` is already a linear `std::vector` with
integer child indices, so "flattened to an array" is the layout you inherit.
`AABB::SurfaceArea()` is there for SAH.

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
- [ ] `BVH::traverse()` — explicit stack, and **tighten `t_best` on every
      accepted hit**; that is where most of the speedup comes from
- [ ] Front-to-back ordered traversal, then upgrade the split to **SAH** — the
      exit criterion asks for SAH, median is the stepping stone
- [ ] Tune `LEAF_SIZE` (currently 4) and measure

Verify with `BVH_VERIFY=1`, which runs both paths on every ray. A BVH that
is merely slow still renders correctly; one that drops triangles makes holes
that are easy to miss by eye.

### Step 9 — Textures

Procedural checker first — it makes UV seams and winding errors visible
instantly. Then image textures with bilinear sampling. Normal maps last.

**Done when:** a textured **OBJ** model matches a reference render, and you
understand why your first normal map attempt looked wrong.

**Restated from glTF deliberately.** There will be no glTF loader by the
deadline, and scoring this step against a loader that is out of scope would
drag step 7 back in through the back door. The OBJ path is the reference
instead.

*Where you stand:* nothing. No `vt` parsing, no UV in the hit record, no
sampler. lodepng is already vendored, so image loading is solved. The first two
are what the restated criterion actually costs — they are step 9's work now,
not step 7's.

### Step 12 — Instancing + motion blur  ◇ *stretch goal*

Transform-instanced geometry sharing one BVH, rays carrying time, transforms
interpolated over the shutter interval. Many instanced spheres with
per-instance motion.

**Done when:** an animated multi-frame sequence renders with correct blur.

**A stretch goal for the deadline, not a requirement** — attempt it only once
steps 5, 8 and 9 have passed. If it is attempted, build the motion-blur
half: time on the ray, transforms interpolated across the shutter. The
shared-BVH instancing path can wait. Nothing in the blur work is blocked on
it — scene-graph instancing already works, and the two share a heading here
only because they were planned together.

*Where you stand:* better than you might expect. `assets/scenes/instance.lua` already
reuses a shared subtree under several parent transforms, so scene-graph
instancing works. The animation pipeline exists — `assets/scenes/final_animation.lua`
drives 85 CSV keyframes through `gr.render` and `scripts/stitch_animation.sh`
turns the frames into a video. Missing: time on the ray, transform
interpolation, and a shared-BVH instancing path.

`assets/scenes/test.lua` has a commented-out `gr.nh_sphere_mb` carrying a velocity
vector — that was the original idea.

---

## 3a. Deferred past the deadline

Steps 6, 7, 10 and 11 are out of scope for the first pass and are parked here
so the sequence above reads as the work actually queued. The renderer is
demonstrably weaker without them — this records what is being given up, not
that it does not matter.

### Step 6 — Multithreading over tiles  ⏸ *after the deadline*

Tile-based job queue, per-thread RNG state (never share a generator), one
shared accumulation buffer.

**Done when:** output is bit-identical to single-threaded at a fixed seed, and
you've measured scaling across thread counts. Expect it to be sublinear — find
out why.

**Deferring this is not free.** 16 threads measured 2.05× on a 20-core
machine, and both step 8's profiling writeup and step 12's animation are
render-time-bound — so every measurement those steps need is slower to take
than it has to be. Deferred anyway, because neither is *blocked* by it.

*Where you stand:* per-thread `std::mt19937` seeded by thread index is done.
Decomposition is still static scanline bands, which gives every thread an equal
number of *rows*, not an equal amount of *work*.

Part of "find out why" is already answered, so don't re-derive it: 16 threads
were measured at only **2.05×** on a 20-core machine. The dominant cause was
memory bandwidth — every ray copied the 3.3 MB background — and that is fixed.
**Re-measure from scratch before drawing conclusions.** What remains is band
load imbalance, which is exactly what tiles fix.

### Step 7 — Triangle meshes + glTF  ⏸ *after the deadline*

cgltf or tinygltf. Triangle intersection (Möller–Trumbore), vertex normals with
interpolation, transform hierarchies flattened to world space.

**Done when:** a real model renders correctly, and you've recorded the frame
time. ~~You need this number for step 8.~~ Step 8 now takes its baseline from
the OBJ path instead, so nothing downstream waits on this.

*Where you stand:* OBJ meshes work; the triangle test is the textbook Cramer's
rule formulation, which already computes beta/gamma and throws them away — you
need those barycentrics for interpolation, so they are half the work already
done. Missing: any glTF loader, `vn` parsing (meshes are flat-shaded today),
and flattening (the graph is walked per ray, transforming rays into local space
rather than geometry into world space).

### Step 10 — Emissive geometry + next event estimation  ⏸ *after the deadline*

Area lights, then explicit light sampling with shadow rays and area-to-solid-
angle PDF conversion.

**Done when:** a small bright light converges in a fraction of the samples it
used to, with the same converged result. Graph both.

*Where you stand:* `Light` is a point-light struct — position, colour, and a
`falloff[3]` that is parsed from Lua and **never read by the shader**.

~~Shadow rays pass `MAX_T` as the far bound, so geometry *behind* the light
casts shadows.~~ **Fixed 22 Sep**, and it took a closed room to expose it. The
shadow ray's direction is `light->position - hit_point`, unnormalized, so the
light sits at `t = 1` and the bound has to be `1.0`; with `kMaxT` anything
past the light occludes it too. Outdoors there is rarely anything past a
light, which is why every existing scene rendered correctly and stayed
byte-identical after the fix. In `cornell_box.lua` the ceiling is always past
a ceiling lamp, so every surface shadowed itself and the frame came out
black. The lesson is not the one-line fix, it is that the scene set had no
closed geometry in it until now.

### Step 11 — Multiple importance sampling  ⏸ *after the deadline*

Combine BSDF and light sampling with the power heuristic.

**Done when:** the furnace test still passes, and rough metal under a large
area light shows no fireflies or dark bands.

*Where you stand:* depends entirely on steps 3 and 10.

---

## 4. Baselines

Release build, MinGW GCC, 20 logical cores, 1 spp unless stated. Two columns,
because they answer different questions: **render** is the figure the renderer
logs (`done in N ms`), **wall** is the whole process. The gap between them is a
fixed ~80 ms of start-up and decoding the 3.3 MB background texture, and it is
not something an accelerator can improve.

| Scene | Resolution | spp | Render | Wall |
|---|---|---|---|---|
| `assets/scenes/simple.lua` (5 spheres) | 256×256 | 1 | 16 ms | ~94 ms |
| `assets/scenes/macho-cows.lua` (17.4k triangles) | 256×256 | 1 | ~3490 ms | ~3580 ms |
| `assets/scenes/final_animation.lua`, one frame | 512×512 | 1 | — | ~230 ms |
| full 85-frame animation | 512×512 | 1 | — | ~20 s |

`macho-cows` over four runs: 3431, 3452, 3532, 3679 ms — call it 3490 ms and
treat anything under 5% as noise. `simple` over three: 20, 16, 16 ms.

### The step 8 "before" number

Re-taken 22 September 2026; see step 8 above for the current table, which
includes triangle-test counts. The original note follows, because its two
corrections still stand.

Taken 13 September 2026, at commit `3b5d72e`, while `bvh not built (linear
scan)` is still the only path — which is the whole point of taking it now.

**`macho-cows.lua`, 256×256, 1 spp, 20 threads, Release: 3490 ms.** That is the
number step 8 has to beat, and it must be re-measured on the same scene, the
same resolution, the same sample count and the same thread count, or it is not
a comparison.

Two corrections came out of taking it, and both change what the writeup can
claim:

- **The scene is ~17.4k triangles, not ~35k.** Three cow instances share one
  5,804-face mesh (17,412), plus a 116-face buckyball, a 2-face floor, and six
  instanced arches of two `nh_box`es each, which `NonhierBox` expands to 12
  triangles apiece. `cow.obj`'s face lines are already triangles, so there is no
  quad split to double them. The old figure was roughly twice the truth.
- **The cow scene is ~218× slower than the sphere scene, not 33×.** The old
  ratio divided two *wall* clocks, and wall clock on `simple.lua` is 83%
  start-up — 94 ms of which only 16 ms is rendering. Comparing the render
  figures gives 3490 / 16. The fixed cost was diluting the very gap the number
  was meant to describe, and it flattered the linear scan by 6.6×.

**The exit criterion asks for rays/sec, and nothing counts rays.** `BVH` has
`g_nodes_visited` and `g_triangles_tested`, but only `Traverse()` would bump
them, and `Traverse()` is unwritten — so `bvh frame totals` reports `0, 0`
today and `LinearScan` contributes nothing to either. A before/after of "0
triangles tested → several million" says nothing at all. If the writeup is to
quote rays/sec or triangles-per-ray, `LinearScan` needs to increment the same
counter the traversal will, and the renderer needs a primary/shadow/bounce ray
count. Both are small, both have to exist **before** the tree does, and neither
exists yet. Frame time is comparable without them; nothing else is.

Steps 6, 7 and 8 all ask for numbers. Record them here as you go, alongside the
scene, resolution, sample count and thread count — a rays/sec figure without
those is not comparable to anything.

---

## 5. Off-staircase backlog

Not on the staircase, but cheap and worth folding in when you are next in the
relevant file.

- [ ] **Light falloff** — `Light::falloff` is parsed and never read. Subsumed
      by step 10, but a two-line win before then.
- [ ] **Stratify the pixel jitter and the aperture disk.** Both are drawn
      independently at random, so `samples x lens_samples` is only a product
      and `lens_samples` means nothing on its own (step 5). Stratifying both
      2-D domains would cut variance at the same ray budget and give the two
      numbers separate meanings. The `TODO` sits at the jitter in
      `src/render/renderer.cc`; the out-of-focus noise in
      `renders/thin_lens_far.png` is what it would fix.
- [ ] **`RenderBand`'s sample count is named three things** — `chunk` at the
      call site, `passes` as the parameter, `pass_offset` for the count already
      traced, against `total_samples` and `g_samples_per_pixel` for the budget.
      One quantity, and the parameter names the loop rather than the contract.
      `samples_to_add` / `samples_done`, and `start_row` / `end_row` for the row
      indices while in there. File-local, so it is contained.
- [x] **Thread count** now from `std::thread::hardware_concurrency()`.
- [ ] **Background filename** is hardcoded to `"kh_stain_glass.png"` in
      `src/render/renderer.cc`; should be a scene parameter. Subsumed by step 3's environment
      light.
- [x] **`RayTracer` is a `Ray`** — renamed. Making the getters `const` (and
      so the whole `IsHit` chain, removing the `const_cast` in `Sphere::IsHit`)
      is still open.
- [ ] **`Primitive::IsHit` returns `false`** instead of being pure virtual, so
      a primitive that forgets to override it silently renders nothing.
- [ ] **`NonhierBox` builds a 12-triangle mesh** per box. A box *is* an AABB —
      once step 8's slab test exists, boxes get an analytic intersection free.
- [ ] **Two different `EPS`** — `kEpsilon` is `1e-6`, declared in
      `src/render/sampling.h` and used by `src/render/renderer.cc`; `kEps` is
      `1e-5` in `src/geometry/mesh.cc`. Both are now cross-referenced in the
      source.
- [x] **`using namespace std/glm` in three headers** — removed, from the
      five `.cc` files that had one too. Names are qualified (`glm::vec3`,
      `std::vector`), which is what the Google style pass required. It
      also removed a live hazard: our `Reflect` and `glm::reflect` take
      the same argument types but use opposite sign conventions, and
      until the directive went, overload resolution decided which one a
      call got.
- [ ] **`Image` stores 3 `double`s per pixel** (24 bytes; 25 MB at 1024²).
      Revisit at step 1, when the accumulation buffer is designed.
- [ ] **`JointNode`** is A3 vestigial — bound as `gr.joint`, no `IsHit`
      override, unused by any scene.
- [ ] **`RENDER_BOUNDING_VOLUMES`** is a compile-time define branched on at
      runtime in the hot path.
- [x] **`premake4.lua`** removed — the old build system, redundant now CMake
      works.
- [x] **Scene clutter** — `sample.lua`, `nonhier.lua`, `simple-cows.lua` and
      `mucho-macho-cows.lua` deleted: A3 leftovers, unreferenced by any doc,
      test or source file. `glass_spheres_camera_near/_far.lua` differed only
      in fov, and are now one `glass_spheres_camera.lua` that renders both
      framings — verified byte-identical to the two it replaced. 19 scenes to 14.
- [x] ~~The renderer translation unit~~ — renamed to `src/render/Renderer.*` and its
      public entry points (`Render`, `SetLens`, `SetSamplesPerPixel`,
      `SetSnapshotInterval`, `SetOutputPath`) lost their coursework prefix.

**Open question:** step 7 introduces glTF, which overlaps with what the Lua
scene layer does today. Decide then whether Lua stays as the scene/animation
driver with glTF only for geometry, or whether glTF takes over. The animation
pipeline currently depends on Lua.

Deliberately kept: `polyroots.cc` is 1079 lines of which only
`QuadraticRoots` is called, but its cubic and quartic solvers are most of the
work for torus and cone primitives.

---

## 6. Repo notes

- **Extracted from the CS488 coursework repo.** It lived in a subdirectory of
  `IbukunSanni/computer-graphics-portfolio`; `git subtree split` preserved all
  31 commits of its history.
- **Standalone build.** The renderer is CPU-only and never needed OpenGL — the
  old build linked it against the course framework (and so GLFW, ImGui and
  OpenGL) for a single 19-line header, `math_utils.h`. That header now lives
  here and the GL stack is gone entirely.
- **Vendored** under `third_party/`: glm, lodepng, Lua 5.3.1.
- **Sources are grouped under `src/`** by concern (core, math, geometry,
  scene, render, lua), with includes written relative to `src/`. Moved with
  `git mv`, so `git log --follow` still works.
- **Warning-free** under GCC (`-Wall`) and MSVC (`/W3`). Their renders agree to
  within 0.154% of pixels on `simple.lua`, not exactly — a byte-identical
  comparison is only valid within one toolchain.
- No top-level licence chosen yet — see the README's provenance note.

