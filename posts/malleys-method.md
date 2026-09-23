---
title: "A Hexagonal Aperture Would Break Every Diffuse Surface in My Renderer"
published: false
description: My camera's aperture sampler and my diffuse BSDF sampler are the same function. A bladed iris would corrupt every diffuse bounce -- and my energy-conservation test can't see it.
tags: graphics, cpp, math, rendering
cover_image:
---

<!-- COVER: docs/images/step5-rack-focus-near.png (exists) -- the thin-lens
     render this aperture belongs to. Upload it and paste the URL into
     cover_image: above. dev.to crops covers to 1000x420. -->

I wanted hexagonal bokeh. Real irises have blades, out-of-focus highlights come
out hexagonal, and in a path tracer it's a one-line change: sample a hexagon on
the aperture instead of a disk.

I can't have it yet. That one line would corrupt every diffuse surface in my
renderer -- and the test I'd trust to catch it stays green.

The two call sites look unrelated:

```cpp
// The camera: a physical aperture.
const glm::vec2 lens_uv = SampleUnitDisk(rng) * cfg.aperture_radius;

// The diffuse BSDF: nothing here is disk-shaped.
const glm::vec2 d = SampleUnitDisk(rng);
const float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
return d.x * tangent + d.y * binormal + z * normal;
```

Same function.

## Why the BSDF borrows a disk

The second snippet is **Malley's method**, and it's textbook: pbrt shares one
disk sampler between its lens and its cosine hemisphere on purpose. I got there
by accident -- I wrote the lens first and found it was already my BSDF sampler.

It works because solid angle projects onto the disk with a factor of cosine,
$dA = \cos\theta \, d\omega$, so a uniform disk sample lifted to the hemisphere
has density

$$
p(\omega) = \frac{\cos\theta}{\pi}
$$

For a Lambertian surface, whose BRDF is $\rho/\pi$, the estimator then cancels
completely:

$$
\frac{f_r \cos\theta}{p(\omega)} = \frac{(\rho/\pi)\cos\theta}{\cos\theta/\pi} = \rho
$$

Each bounce multiplies throughput by exactly the albedo. That's what makes it
fast. It's also what hides the bug.

## Any sampler is correct -- if `Pdf()` tells the truth

The estimator is unbiased for *any* way of choosing directions, as long as the
pdf it divides by is the density the directions were actually drawn from.
Uniform over the hemisphere works. A hexagon-shaped lobe would work too, if its
`Pdf()` described a hexagon. The choice changes the noise, never the answer --
cosine is picked for speed and for that cancellation, not for correctness.

So the hexagon isn't a bug because it's a hexagon. It's a bug because it changes
`Sample()` and leaves `Pdf()` reporting the cosine. And because `Pdf()` is
evaluated on the direction just drawn, each bounce's weight is still exactly
`ρ`. Drawing from a lobe `q` while `Pdf()` reports the cosine converges to

$$
\rho \cdot \mathbb{E}_q[L_{\text{in}}]
\quad\text{instead of}\quad
\rho \cdot \mathbb{E}_{\cos}[L_{\text{in}}]
$$

Those are equal whenever incoming light is the same from every direction --
which is exactly the furnace test. So the prediction before running anything:
the energy test passes, and only checks on the *shape* of the distribution fail.

## I made the change

I swapped a hexagon into `SampleUnitDisk` -- two lines -- and ran every test.

<!-- IMAGE: docs/images/malley-hexagon.png (exists). Regenerate it, and every
     number in this post, with the command at the top of
     scripts/malley_figure.cc. Upload, then replace this comment with:
     ![Left: the unit disk and the hexagon a bladed iris puts inside it. Right: the distribution of cos θ after the lift.](URL)
     *Right panel: the disk (blue) follows the exact cosine lobe (grey); the hexagon (orange) doesn't. Its jump at cos θ = 0.5 is the hexagon's apothem.* -->

| Check | Compares | Real sampler | Hexagon |
|---|---|---|---|
| Furnace, 10 render tests | pixels vs environment | pass | **pass** |
| `Pdf()` mass vs draws above horizon | the total | 1.0002 vs 1 | **pass** |
| Mean cosine | vs 2/3 | 0.6663 | **0.7437 -- fail** |
| `cos²/pdf` integral | vs 2π/3 ≈ 2.0944 | 2.0943 | **2.3373 -- fail** |
| Blinn-Phong `cos²/pdf`, 4 angles | vs 2π/3 | pass | **fail at all 4** |
| Two constructions agree | Malley vs a second sampler | 0.66689 vs 0.66682 | **0.74395 vs 0.66682 -- fail** |

The top two rows check totals, and both passed on the wrong distribution. The
bottom four check shape, and all four failed.

**The furnace passed.** All 10 render tests stayed green, and the half-radiance
furnace changed in 8 of 65,536 pixels, by one byte each. `Pdf()` is evaluated on
the direction just drawn, so each bounce's weight is exactly the albedo whatever
the distribution -- and a uniform environment hides the rest.

<!-- IMAGE: docs/images/furnace-lambertian-half.png (exists). Regenerate with
     ctest --test-dir build -R furnace, then copy
     tests/out/furnace_lambertian_half.png. Upload, then replace with:
     ![A flat grey square](URL)
     *The furnace with the hexagon in place. Correct looks like this. So does broken.* -->

**The normalisation check passed too**, and it's the one my notes had said would
catch this. `Pdf()` never changed, and every lifted direction is still above
the horizon, so both sides stay at 1.

**Blinn-Phong went down with it** at every tested angle. Its diffuse half calls
the same sampler -- a second victim I hadn't known about.

## The test that caught it

Before making the change I'd written a test that builds the cosine lobe a second
way -- the way *Ray Tracing in One Weekend* does:

```cpp
const glm::vec3 dir = glm::normalize(normal + RandomUnitVector(rng));
```

That route draws from `SampleUnitBall`, not `SampleUnitDisk`, so it can't move
when the aperture does. Two constructions of one distribution have to agree on
every moment, which makes them a **differential test**: no reference renderer,
no known answer needed. Over 2 million samples each:

| | `E[cos]` | `E[cos²]` |
|---|---|---|
| Malley (disk lift) | 0.66634 | 0.49964 |
| `normalize(n + RandomUnitVector())` | 0.66662 | 0.49999 |
| hexagon lift | **0.74374** | **0.58309** |
| exact | 0.66667 | 0.50000 |

"Agree" has to be a number, not a judgement. The test allows four standard
errors of the difference, a tolerance that tightens as samples grow: the real
pair came in 0.00007 apart against 0.00094 allowed. With the hexagon in, they
were 0.077 apart against 0.0008.

It compares two moments, not one, because a single matching number isn't a
fingerprint. On the unit disk, two different integrals both land on 2/3:

$$
\begin{aligned}
\mathbb{E}[r] &= \int_0^1 r \cdot 2r \, dr = \frac{2}{3} \\
\mathbb{E}\!\left[\sqrt{1 - r^{2}}\right] &= \int_0^1 \sqrt{1 - r^{2}} \cdot 2r \, dr = \frac{2}{3}
\end{aligned}
$$

Only the second is `E[cos θ]`. I nearly published the first as proof of
Malley's method.

One limit: two samplers that share a bug agree with each other and are both
wrong -- these two draw from the same random number generator. That's why the
closed-form checks stay in the suite. The differential test says the two routes
match; the closed forms say they match the right answer.

## Mechanism versus coincidence

The two routes start from sibling primitives. `SampleUnitDisk` and
`SampleUnitBall` are the same **mechanism** one dimension apart: sample the
enclosing box, reject what lands outside the shape. What you get comes from the
map you apply afterwards:

| Primitive | Then | Gives | Used by |
|---|---|---|---|
| `SampleUnitDisk` | nothing | uniform disk | camera aperture |
| `SampleUnitDisk` | lift `z = √(1−r²)` | cosine hemisphere | Lambertian, Blinn-Phong |
| `SampleUnitBall` | normalize | uniform sphere | rough-metal fuzz |
| `SampleUnitBall` | normalize, add the normal, normalize | cosine hemisphere | the differential test |

That's the general shape of any sampler: a uniform source plus a map, with the
pdf set by how the map stretches area. The cosine in Malley isn't added -- it
falls out of the projection. The difference even shows in the code:
`SampleUnitBall` carries a lower bound on length because the next step divides
by it, while the lift never divides at all. Its output is unit length by
construction, off by at most 6.19e-08 across 2 million samples.

The aperture and the BSDF share something else: a **coincidence**. Both want a
disk, for unrelated reasons. The aperture wants one because that's the shape of
the hole; the BSDF wants one because the projection happens to produce a cosine.
Sharing a mechanism is deduplication. Sharing a coincidence is a bug on a delay.

## What I do differently now

- **Share on mechanism, never on coincidence.** When two callers reach one
  function only because their needs look alike, write that down where the
  function is. The lens now carries that comment, and it gets its own sampler
  *before* anyone shapes it.
- **Test the shape, not the total.** Energy and normalisation checks both
  passed on the wrong distribution.
- **Make the change and watch what fails.** My notes named two tests that would
  catch this. Neither did.

---

*From a path tracer I'm building in C++. Next: getting total internal reflection
wrong in four different ways before getting it right.*
