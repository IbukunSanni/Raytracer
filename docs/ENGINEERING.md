# Engineering writeups

Notes captured while the work is fresh, for the engineering blog. Each entry is
a skeleton, not a draft -- enough to write the post later without re-deriving
anything.

---

## Deadline

**The renderer has to be working by 30 September 2026.** Working is defined, and
it is deliberately not the whole staircase:

- ~~**Step 4** -- refraction and reflection~~ **done 13 Sep**: η² scaling, then
  caustics, measured at 1.98x floor brightness
- ~~**Step 5** -- thin-lens camera and defocus blur~~ **done 22 Sep**: rack
  focus measured at 0.99x pinhole sharpness in focus and 0.32x out; a mirrored
  camera basis and a half-pixel pixel-grid offset fixed on the way
- **Step 8** -- BVH
- **Step 9** -- textures

**Stretch goal: step 12, motion blur.** Moved out of the required list on 22
September, with eight days left and nothing committed since the 13th.
Nothing else still planned depends on it, and it was last in the order anyway,
so it can slip without taking anything down with it. Attempt it only once 5, 8
and 9 have passed their exit criteria.

**Steps 6, 7, 10 and 11 -- multithreading, glTF, next event estimation, MIS --
are out of scope for the 30th.** The staircase's "do not start N+1 until N
passes" rule is being broken on purpose to skip them. The work continues after
the date; this is the line for calling the first pass done.

Three of the steps still planned have exit criteria written against work that is
now out of scope. Restate them before starting, or they will drag step 7 back in
through the back door:

- **Step 9** says *a textured glTF model matches a reference render*. There will
  be no glTF loader by the 30th. Score it against an OBJ model instead -- which
  still needs `vt` parsing and a UV in the hit record, neither of which exists.
- **Step 8** wants a before-number that step 7 was going to produce. Take it from
  the existing OBJ path: `macho-cows.lua` is 17.4k triangles and logs `bvh not
  built (linear scan)` today, so it is already the right scene to measure.
- **Step 12** is written as instancing *and* motion blur over a shared BVH. The
  blur half -- time on the ray, transforms interpolated across the shutter -- is
  what the stretch goal needs. The shared-BVH instancing path can wait.

**Step 6 stays out, but know what that costs.** 16 threads measured 2.05× on a
20-core machine. Both the step 8 profiling writeup and the step 12 animation are
render-time-bound, so skipping tiles makes every remaining measurement slower to
take.

---

## Publishing plan

**Ship this as five posts, not one.** A single write-up at the end means one
publication, written when the details have gone cold, about work whose
intermediate states no longer exist. Five posts published as the work lands
means the deadline produces a series instead of an artefact -- and each post is
written the week its bugs are still fresh.

The parts below already match the natural boundaries of the remaining work, so
this costs no extra effort. It only requires deciding *before* each part that
the images and the commit range belong to it.

### The mechanic

Two habits make a part self-contained. Both have to happen while the work is
live; neither can be reconstructed afterwards.

**Tag the boundary.** When a part is done, `git tag post-N-<slug>`. The commit
range for that post is then `post-(N-1)..post-N`, which gives you the diff and
the order things actually happened in.

It does **not** give you the narrative. Commit messages here are capped at
three lines on purpose, so `git log` scopes a post but cannot draft one. The
prose has to be written when the work lands, into the docs: `ROADMAP.md` for
what changed and why it was next, this file for derivations, measurements and
bug stories. A part with no entry here by the time it is tagged is a part that
will be reconstructed from memory later, badly.

**Put the images in `docs/images/`.** This is not optional bookkeeping:
`renders/**/*.png` is in `.gitignore`, so anything left in `renders/` is not
tracked and will not survive. A render intended for a post has to be copied to
`docs/images/` deliberately.

**Capture the wrong image too, at the moment it is wrong.** The most valuable
picture in any of these posts is the broken one -- the black sphere, the
missing TIR, the fireflies. Once fixed it is gone forever, and no amount of
writing reconstructs it. Screenshot the failure before fixing it, every time.
This is the single habit most likely to be skipped and most regretted.

### The five parts

| # | Post | Lands | Hero image |
|---|---|---|---|
| 1 | Energy conservation, and how to prove it | done | the flat grey furnace render |
| 2 | Refraction in five rungs | week 1 | one render per rung |
| 3 | Making it move | week 2 | the ugly 16 spp draft |
| 4 | The BVH, and where the time actually goes | week 3 | rays/sec table, depth heatmap |
| 5 | The final render, and everything that broke | final stretch | the finished shot |

**Part 1 -- Energy conservation, and how to prove it.** Publishable now; the
work is committed. The BSDF interface, the furnace test at two levels, and the
two things worth a reader's time: why the obvious `f*cos/p` sampler check is
*degenerate* for a Lambertian -- it returns the albedo even when `Sample()`
draws garbage, because the pi and the cosine cancel algebraically -- and
Malley's method, where `E[cos] = 2/3` on the hemisphere is literally the same
integral as `E[sqrt(1-r^2)]` on the disk. Deleting the half-vector Jacobian
fails 8 of 8 sampler checks, which is the proof the tests bite. The hero image
is a flat grey square, and the caption is the joke: this is what correct looks
like.

**Part 2 -- Refraction in five rungs.** The rungs in ROADMAP step 4 each
require a render to verify, so the image sequence is free: mirror sphere,
Fresnel rim brightening at grazing, transmission, TIR. The ideas are a
dielectric reflecting *and* refracting rather than one or the other; delta
pdfs and why `Eval()`/`Pdf()` correctly return 0 for a mirror (the same
zero-measure argument that stops BSDF sampling ever hitting a point light,
pointing the other way); and the eta^2 radiance scaling across an interface,
whose exponent is set by what the renderer transports rather than by the
physics statement -- and which the furnace cannot score at all, in a way that
is a better story than catching it would have been.

**Part 3 -- Making it move.** The animation pipeline, the camera path, and the
discipline of rendering the whole sequence badly before making any frame good.
Shorter than the others. Fine.

**Part 4 -- The BVH, and where the time actually goes.** Step 8's checkpoint
ladder is already written as a list, and it is a post outline as it stands:
median split, the checkpoint where `AABB::hit()` still returns `true` and the
image must be unchanged, the slab test, tightening `t_best`, then SAH. Needs
before/after rays-per-second on the same scene and an explanation of where the
remaining time goes. This is the one a tools company reads most closely.

**Part 5 -- The final render, and everything that broke.** The shot, a short
architecture note, and the bug collection: the shadow ray's `MAX_T` letting
geometry behind a light cast shadows, the 372x background copy, the chained
comparison `i < loopMAX < 4` that accidentally implemented textbook rejection
sampling. The `## Candidates` list below is the raw material -- entries move up
here as they are used.

*(The double-transform entry under `## Queued` is the odd one out in this
collection: it is the only bug here that never produced a wrong render, so it
argues for reading over debugging rather than the reverse. If part 5 gets
crowded, it is the one that can stand alone as its own short post.)*

---

## Queued

### Malley's method: the lens sampler turned out to be the hemisphere sampler

**The hook.** `SampleUnitDisk()` was written for depth of field -- step 5, the
thin-lens camera, where the disk is a physical aperture. It turned out to be
the whole of cosine-weighted hemisphere sampling for step 3's BSDF, where
nothing in the scene is disk-shaped at all.

**The claim.** Take a uniform sample on the unit disk and project it straight
up:

    (x, y) uniform on the unit disk
    z = sqrt(1 - x^2 - y^2)
    -> (x, y, z) is cosine-distributed on the hemisphere

**Why it holds.** Solid angle projects onto the disk as `dA = cos(theta) dw`.
A uniform disk has density `1/pi` per unit area. So

    p(w) = (1/pi) * cos(theta)

which is exactly the cosine pdf. No rejection step, no trig, one sqrt.

**Why it matters -- the cancellation.** Lambertian `fr = rho/pi`, cosine pdf
`cos/pi`, and the Monte Carlo estimator wants `fr * cos(theta) / pdf`:

    (rho/pi) * cos(theta)
    --------------------- = rho
        cos(theta)/pi

Everything cancels. Throughput multiplies by albedo per bounce and nothing
else. That is why the furnace test works: albedo 1 means throughput stays 1
forever, so the sphere returns exactly the environment radiance and vanishes
into it. The test is a direct probe of whether `fr`, `pdf` and the cosine
agree -- dark means a lost factor, bright means one counted twice.

**The trap worth writing about.** One primitive, two consumers, for unrelated
reasons: the lens because an aperture transmits uniformly over its area,
Malley because the projection happens to produce cosine weighting. Coincidence,
not shared physics. A shaped aperture -- hexagonal bokeh, a bladed iris -- is a
correct change for the lens and would silently break the BSDF, whose `Pdf()`
would stop matching what `Sample()` draws. **The furnace does not catch it.**
Measured by mutation on 22 Sep: with a hexagon in `SampleUnitDisk`, all 10
render tests pass, because the Lambertian weight is exactly rho for any drawn
direction and a uniform environment cannot tell where directions went. The
sampler moment tests catch it. Written up in `posts/malleys-method.md`.

**Verification.** 2M samples through the disk sampler:

    E[r]   = 0.66668   (uniform disk: 2/3)
    E[r^2] = 0.50000   (uniform disk: 1/2)
    E[x]   = -0.00006  E[y] = -0.00039
    quadrants: 499855 499219 499872 501054

---

### The transform that was applied twice, in a scene that could not show it

**What was observed.** Nothing. Every scene in the repo rendered correctly
and still does -- `hier`, `instance`, `nonhier`, `macho-cows` and `simple`
all came out byte-identical across the fix. No scene nested anything under a
`GeometryNode`, and that is the only configuration where the bug fires. It
was found by reading `GeometryNode::IsHit` during the Step 0 sweep, not by
looking at a wrong picture. That is the reason this one is worth writing up:
every other bug story here starts with a bad render.

**What the code assumed.** That `SceneNode::IsHit` was a reusable "intersect
my children" helper, so a `GeometryNode` could handle its own primitive and
then delegate the rest upward. The naming actively encourages it -- the
base-class method is the generic one, so calling it from the derived class
reads like reuse rather than like a second pass.

**Why that is wrong.** `SceneNode::IsHit` was never "intersect my children".
It was *transform into my space, intersect my children, transform back* --
three steps welded into one function, of which only the middle one was what
`GeometryNode` wanted. Delegating to it ran the transform a second time:

    GeometryNode::IsHit(ray)
      local  = T^-1 * ray            <- first application
      primitive->IsHit(local)        <- correct, sees T^-1
      SceneNode::IsHit(local)
        local2 = T^-1 * local        <- second application, same matrix
        children->IsHit(local2)      <- wrong, children see T^-2
        record = T * record          <- restored twice, symmetrically
      record = T * record

The restore is doubled too, so what comes back is self-consistent: no NaN, no
corrupt normal, no missing geometry. The child is simply somewhere else. For
the test scene's parent translate of `(150, 0, -400)`, a child authored at
`(0, 150, 0)` is intersected as though it sat at `(0,150,0) + 2T =
(300, 150, -800)` -- displaced by exactly one extra copy of the parent
transform, and pushed 400 further from an eye at `z = 800`, so smaller with
it. *(Derived from the matrices, not yet measured -- see the open item.)*

**The second bug, in the same function.** The child loop ended with

    localRecord.material = geometryNode->m_material;

set unconditionally after the recursive call. So a hit that actually landed
on a *nested* child returned the parent's material. In the test scene the
child sphere is orange and its geometry-node parent is green, so before the
fix the child rendered green -- displaced *and* wearing the wrong material,
two independent defects with one symptom. The right rule is that the material
belongs to whichever `GeometryNode` owns the primitive that was hit, and
nobody above it gets to overwrite that.

**The fix.** Split the welded function into its three parts and let both node
types share them: `ToLocal` on the way in, `ToWorld` on the way out, and
`HitChildren` which does *not* transform at all, because its caller already
has and each child applies its own transform inside its own `IsHit`. The
transform is now applied exactly once per node by construction rather than by
everyone remembering not to. `GeometryNode::IsHit` also stopped reusing one
`HitRecord` across the primitive and the children -- they get separate
records now, which is what stopped the material leaking between them.

**What transfers.** Three things, none of them about raytracing:

1. **A function that does setup, work, and teardown cannot be called for the
   work alone.** The bug is not in any line of arithmetic -- every matrix
   here is correct. It is in a function boundary drawn in the wrong place.
   The fix is a decomposition, not a correction.
2. **Inheritance made the wrong call look like the right one.** If the helper
   had been a free function named `TransformIntersectRestore`, nobody would
   have called it from a node that had already transformed. `SceneNode::IsHit`
   reads as the generic case of what `GeometryNode::IsHit` specialises, and
   that resemblance is the entire trap.
3. **A latent bug is a bug with a deadline.** This one fires the first time
   anyone parents anything to a geometry node -- which is exactly what
   instancing in step 12 does. Found by reading it cost an afternoon; found
   by rendering it, mid-step-12, it would have looked like a broken instancing
   implementation and been debugged in the wrong file.

**Verification.** Two directions, and both are needed. *Unchanged where it
should be:* all five existing scenes render byte-identically across the fix,
which is what proves the refactor did not move anything that was already
right. *Changed where it should be:* a new equivalence pair,
`tests/scenes/nested_control.lua` and `nested_under_geometry.lua`, describes
the same picture two ways -- one child under a plain `gr.node`, the same
child under a `GeometryNode` carrying the same translate. They must come out
identical; before the fix they differed by 1265 bytes. Byte equality is the
honest bar here, because the two scenes are the same picture, so any
difference at all is the bug. Guarded by
`tests/render_test.cc`, suite `render/scene-graph`.

**Open items before this ships as a post.**

- The 1265-byte figure is what the fix commit recorded, and it is a raw byte
  count. Re-take it through the current `Image` helper as *pixels differing*
  and *max channel delta*, which is the unit every other measurement in these
  posts uses.
- **The broken image is recoverable, which is rare.** The standing rule here
  is to screenshot the failure before fixing it, because it is gone forever
  afterwards. Not this time: the bug is deterministic and the revert is two
  functions, so rendering `nested_under_geometry.lua` against a reverted
  `SceneNode` reproduces the wrong picture exactly. Do that once and put it in
  `docs/images/` -- side by side with the control, it is the whole post in one
  frame, and it is the only hero image this story can have.

---

### The caustic you cannot render, and the two reasons why

**What was observed.** A correct dielectric -- Snell, TIR, exact Fresnel, η²
scaling, every one of them under test -- and no caustic. `glass_spheres.lua`
renders a glass sphere that refracts the scene behind it perfectly and casts
nothing onto the floor beneath it at any sample count.

**What the plan assumed.** That caustics were a *sampling* problem. The roadmap
had carried a standing warning for weeks: Russian roulette kills paths in
proportion to throughput, a glass path spends several bounces before it
delivers anything, so caustics are the paths roulette cuts first -- *if they
look sparse, raise `kRrStartDepth` before doubting the BSDF*.

**Why that is wrong.** Measured, not argued. Rebuilt with `kRrStartDepth` at 8
instead of 3, the same 512 spp render of the same scene gives a core/floor
ratio of 1.98× either way, floor noise of 12.2% either way, and the same frame
time. Roulette was never touching these paths, and the reason is one line:

    q = min(0.95, max(throughput))

Roulette kills **dim** paths. A caustic path is a bright one -- floor albedo
0.75, two dielectric branches weighing about 1 -- so `q` sits near 0.75 and the
path survives. The advice had the direction of the effect backwards.

**The real reason, which is about the light source and not the material.**
A caustic needs a *concentrated* source, and of the three this renderer offers,
two cannot carry one at all:

1. **A point light cannot.** `RayTraceRgb`'s crude NEE loop casts a shadow ray
   from the hit point to the light and skips the light if anything is in the
   way. A dielectric is "anything". So glass in front of a point light produces
   a **shadow** -- the caustic path is exactly the one the occlusion test throws
   away. This is not a bug to fix; recovering it needs photon mapping or
   bidirectional tracing, neither of which is anywhere in the plan.
2. **A uniform environment cannot**, and the proof is the furnace test's own
   argument turned around: refraction redistributes uniform radiance into
   uniform radiance, so the focus carries no more energy than the floor beside
   it. The flat sky in `glass_spheres.lua` makes a caustic *invisible by
   construction*. Nothing was broken.
3. **A small bright region in a lat-long map can**, because BSDF sampling can
   reach it: floor → diffuse bounce → glass → two refractions → sun.

So `assets/scenes/caustic.lua` exists to make the third case: a 4°-radius sun
in a generated environment map, a glass ball at `ior = 1.5` floating 0.3 above
a diffuse floor. A ball lens focuses at `nR / (2(n-1))` = 0.75 from its centre,
which puts the focus at `y = -0.45` against a floor at `-0.5` -- close enough
that the spot is a point rather than a smear. The measurement across the focal
row:

    open floor        74.7
    shadow annulus    60.9     82% of floor
    caustic core     147.7    1.98x floor, 2.43x annulus

Dark ring, bright core. `docs/images/step4-caustic.png`.

**What transfers.**

1. **"It looks wrong" and "it cannot look right" are different diagnoses, and
   the second one is not a bug.** Weeks of standing advice pointed at the
   renderer's termination heuristic. The actual answer was that two of the
   three scene configurations available made the phenomenon unobservable in
   principle. No amount of tuning finds that; only asking *what would have to
   be true for this to be visible* does.
2. **A test's blind spot travels.** The furnace is blind to refraction because
   uniform radiance in gives uniform radiance out. That is a documented
   property of the *test* -- and it turns out to be a property of any uniformly
   lit *scene*, which is why the demo scene could never have worked. The same
   sentence explains a test limitation and a rendering limitation.
3. **A prediction that survives long enough stops being read as a prediction.**
   The roulette warning was written before the dielectric existed and was
   restated every time the section was edited. It was never measured until it
   was cheap to measure -- one constant, one rebuild, one render, about four
   minutes -- and it was wrong.

**Open items before this ships as a post.**

- The noise floor is 12.2% at 512 spp, because the sun subtends 0.0153 sr and a
  cosine-weighted bounce finds it 0.49% of the time. That number is the
  argument for next event estimation (step 10, deferred) and would make a clean
  before/after pair if step 10 is ever built. Re-render then.
- ~~The environment map is generated by a throwaway script.~~ Done:
  `scripts/make_sun_sky.py` writes it, and regenerating from the script
  reproduces 74.7 / 60.9 / 147.7 exactly. A measurement whose input cannot be
  regenerated is not a measurement.

---

## Candidates

Already have the numbers or the story; not yet written up.

- **The portable engine and the unportable distribution.** *(Fixed; the
  numbers below are the before, and the write-up has its own ending now.)*
  Found while checking that the Google style pass changed nothing:
  the GCC build matched its baseline exactly, the MSVC build of the same
  source did not. On `simple.lua` at 256x256, 8.1% of pixels differ and some
  by a full channel -- too large for rounding drift. The cause is one line:
  `Rng` draws through `std::uniform_real_distribution`, whose *algorithm* the
  standard never specifies, so libstdc++ and MSVC's STL return different
  sequences from an identically seeded `std::mt19937`. The engine is portable;
  the distribution is not. Every other determinism guarantee in the renderer
  is intact -- same seed, same thread count, same scanline bands -- which is
  what makes it a good story: the one non-deterministic thing is the piece
  that looks most like library boilerplate. The fix is to transform the
  engine's output directly and stop using the distribution, but it changes
  every render this project has produced, so it wants to be a deliberate
  commit rather than a drive-by. Note the deferred `double` entry above
  assumed this claim still held; it did not.

  **The ending.** `Rng::Next` now takes the top 24 of mt19937's 32 bits --
  exactly a float's mantissa -- and scales by 2^-24, which is exact, so
  nothing rounds and the result can never reach 1. That took `simple.lua`
  from 8.07% of pixels disagreeing (max channel delta 255) to 0.154% (max
  173), and eight of the ten verification scenes now match byte for byte.
  Two things make it a better story than "library was wrong". First, MSVC's
  output did not move at all: MSVC was already computing that formula, and
  libstdc++ was the one doing something else, so the "portable" side was
  whichever one you happened to test on. Second, the FMA theory was wrong
  and worth showing as wrong -- `-ffp-contract=off` changed nothing, because
  the default `-march=x86-64` has no FMA instruction to contract into. The
  flag stayed anyway, as a guard for anyone who builds with `-march=native`.
  What remains is 101 pixels in `simple.lua` and one pixel in
  `nonhier2.lua`: a float comparison in the sampler landing on either side
  of a boundary, one build taking a rejection the other does not, and that
  pixel's sample sequence desynchronising from there. Different class of
  problem, and the honest end of the post is that it is still open.
- **Two bugs that are invisible in the build that has them.** The camera
  basis was mirrored — `u = cross(up, view)` is screen *left*, `v` is screen
  *down* — and `RenderBand` cancelled both by stepping `(w - x)` and `+y` from
  a top-right corner, so the image came out upright and nothing looked wrong
  for the life of the project. Underneath it, the pixel grid sampled pixel
  *corners*: no `+0.5`, so the frame sat half a pixel out in both axes. Neither
  shows in a render you can look at. Both show immediately once you pick the
  right probe.

  For the half-pixel, the probe is a scene built mirror-symmetric about both
  screen axes, which correct registration must render symmetrically. Mean
  mirror difference, before and after, at two sample counts:

      spp     L-R asym          T-B asym
      400     0.622 -> 0.219    0.623 -> 0.217
      6400    0.592 -> 0.054    0.593 -> 0.053

  16x the samples barely moves the old numbers and drops the new ones by 4x
  (= 1/sqrt(16)). That is the whole argument in one table: a systematic bias
  does not average away, Monte Carlo noise does. For the focal plane, the
  probe is a *deliberately wrong build* — swap the one divisor in
  `ThinLensRay` from `dot(pin_dir, w_vec)` to `length(pin_dir)`, render, revert
  — which turns a flat focal surface into a sphere around the eye and
  photographs the difference: seven identical spheres at one axial distance
  hold 1.00 of their sharpness at frame centre and 0.68 at the edges.
  `docs/images/step5-focal-plane-flat.png` and `-curved.png`. The reusable
  lesson is the method, not the bugs: when a defect cannot be seen, either
  construct an invariant it must violate, or build the wrong version on
  purpose and diff.
- **Instrument the path you are about to delete.** The step 8 "before"
  number was wall-clock only, because `g_triangles_tested` was incremented
  inside `BVH::Traverse` and the tree does not exist yet — so on the linear
  scan it read zero. Once the tree lands, that measurement is gone for good.
  Adding one atomic add per scan first gives the real headline: **3.26
  billion** triangle tests for `macho-cows` at 256x256 and 1 spp, which is
  **49,815 per pixel**. The counter costs nothing measurable; the
  uninstrumented build actually timed *slower*. Which is the second half of
  the story — the same uninstrumented binary measures ~3960 ms today against
  the 3490 ms recorded on 13 September, so the machine moved 7% while the
  code stood still. A speedup claim measured against a number from another
  day is not a measurement. Re-take the before alongside the after, or do not
  quote a ratio.
- **The 372x background copy** -- 28,316 ms -> 76 ms, byte-identical output.
  Measurement, root cause, and the proof that nothing changed.
- **`i < loopMAX < 4`** -- a chained comparison that is always true, so the
  rejection loop ran unbounded and accidentally implemented the textbook
  algorithm. Correct output, two GCC warnings, unreachable fallback.
- **Per-ray heap allocation** in `Sphere` / `Cube` / `NonhierBox`.
- **Shared `rand()` -> per-thread `std::mt19937`** -- a correctness and
  contention story, not just a speed one.
- **The DoF/AA double-count** that the `.1 *` fudge factor was hiding.
- **The epsilon that rounded to nothing** -- every furnace sphere came out at
  0.42x its environment at once, while transmission was being added, so it read
  as a dielectric bug. It was in the sphere intersector. `kEpsilon` is `1e-6`
  absolute, but one float ULP at the furnace sphere's `z = -500` is `3e-5`:
  `hit_point + N * kEpsilon` rounded straight back to `hit_point`, so every
  scattered ray restarted exactly on the surface it had just left. `C` was then
  sign-random noise -- 49.1% of surface points read as *inside*, 44.3% took the
  far root and tunnelled through the sphere. Scaling the offset to `|P|` takes
  both to 0.0%. Hero image: the grey furnace square with a darker disc in it.
  - **Deferred: carrying the renderer in `double`.** Also fixes it outright
    (0.0% at a flat `1e-6`) and would let the simpler `C < 0` inside-test
    stand. Not done -- it is a type change across `Ray`, `HitRecord`, the BSDF
    interface and `Framebuffer`, it doubles BVH traversal memory traffic in the
    hot loop, and it would widen the GCC/MSVC render gap that `Rng::Next`
    narrowed to 0.154%. Note that it raises the threshold rather than removing
    it, where the scaled offset is scale-invariant. Revisit only if a scene
    needs detail finer than ~1e-7 of its own extent, where float cannot hold
    the geometry at all.
