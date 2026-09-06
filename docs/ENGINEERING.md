# Engineering writeups

Notes captured while the work is fresh, for the engineering blog. Each entry is
a skeleton, not a draft -- enough to write the post later without re-deriving
anything.

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
