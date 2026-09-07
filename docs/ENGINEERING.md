# Engineering writeups

Notes captured while the work is fresh, for the engineering blog. Each entry is
a skeleton, not a draft -- enough to write the post later without re-deriving
anything.

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
*degenerate* for a Lambertian -- it returns the albedo even when `sample()`
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
pdfs and why `eval()`/`pdf()` correctly return 0 for a mirror (the same
zero-measure argument that stops BSDF sampling ever hitting a point light,
pointing the other way); and the eta^2 radiance scaling across an interface,
which the furnace test catches immediately if you forget it.

**Part 3 -- Making it move.** The animation pipeline, the camera path, and the
discipline of rendering the whole sequence badly before making any frame good.
Shorter than the others. Fine.

**Part 4 -- The BVH, and where the time actually goes.** Step 8's checkpoint
ladder is already written as a list, and it is a post outline as it stands:
median split, the checkpoint where `AABB::hit()` still returns `true` and the
image must be unchanged, the slab test, tightening `tBest`, then SAH. Needs
before/after rays-per-second on the same scene and an explanation of where the
remaining time goes. This is the one a tools company reads most closely.

**Part 5 -- The final render, and everything that broke.** The shot, a short
architecture note, and the bug collection: the shadow ray's `MAX_T` letting
geometry behind a light cast shadows, the 372x background copy, the chained
comparison `i < loopMAX < 4` that accidentally implemented textbook rejection
sampling. The `## Candidates` list below is the raw material -- entries move up
here as they are used.

---

## Queued

### Malley's method: the lens sampler turned out to be the hemisphere sampler

**The hook.** `sampleUnitDisk()` was written for depth of field -- step 5, the
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
correct change for the lens and would silently break the BSDF, whose `pdf()`
would stop matching what `sample()` draws. The furnace test would then fail for
a reason that looks nothing like its cause.

**Verification.** 2M samples through the disk sampler:

    E[r]   = 0.66668   (uniform disk: 2/3)
    E[r^2] = 0.50000   (uniform disk: 1/2)
    E[x]   = -0.00006  E[y] = -0.00039
    quadrants: 499855 499219 499872 501054

---

## Candidates

Already have the numbers or the story; not yet written up.

- **The 372x background copy** -- 28,316 ms -> 76 ms, byte-identical output.
  Measurement, root cause, and the proof that nothing changed.
- **`i < loopMAX < 4`** -- a chained comparison that is always true, so the
  rejection loop ran unbounded and accidentally implemented the textbook
  algorithm. Correct output, two GCC warnings, unreachable fallback.
- **Per-ray heap allocation** in `Sphere` / `Cube` / `NonhierBox`.
- **Shared `rand()` -> per-thread `std::mt19937`** -- a correctness and
  contention story, not just a speed one.
- **The DoF/AA double-count** that the `.1 *` fudge factor was hiding.
