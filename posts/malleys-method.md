---
title: "A Hexagonal Aperture Would Break Every Diffuse Surface in My Renderer"
published: false
description: My camera's lens sampler and my diffuse BSDF sampler depend on the same function. A bladed iris would corrupt every diffuse bounce.
tags: graphics, cpp, testing, math
---

<!-- COVER: docs/images/step5-rack-focus-near.png (exists) -- the thin-lens
     render this aperture belongs to. Upload it and paste the URL into
     cover_image: above. dev.to crops covers to 1000x420. -->

After experimenting with many features in my raytracing journey.
Refrection, reflection, actually making the ray tracer.

<!-- Sample image showing a simple implemenation. I havea a few images that have it. -->

I got to implementing defocus blur. I am sure my image looks very familair to some of yall.

<!-- Defocus blur similar to raytracing in a weekend -->

Implementation reminded me of the bokeh we see in images, and I had the thought of making my own.

<!-- Example bokeh from movies and pictures -->

So I tested my own with what I have and this what I created.

<!-- Basic image render with circular bokeh -->

However, now that I feel like I can make anything. "sinister emoji". Why not try something interesting. I can make the bokeh any shape I want however.

Apparently all my diffuse surface would suffer greatly because of such a change. My tests almost did not catch it.

The actual function called:

```cpp

inline glm::vec2 SampleUnitDisk(Rng& rng) {
  while(true){
    float x = rng.Range(-1.0f, 1.0f);
    float y = rng.Range(-1.0f, 1.0f);
    if (x * x + y * y <= 1.0f) return glm::vec2(x, y);
  }
}

```

what a hexagonal change would look like:

```cpp
// Regular hexagon inscribed in the unit circle: corners at radius 1, flat top
// and bottom, flat edges at the apothem sqrt(3)/2 ~= 0.866.
constexpr float kApothem = 0.8660254f;

inline glm::vec2 SampleUnitDisk(Rng& rng) {
  while (true) {
    float x = rng.Range(-1.0f, 1.0f);
    float y = rng.Range(-1.0f, 1.0f);
    // WAS: if (x * x + y * y <= 1.0f) return glm::vec2(x, y);
    if (std::fabs(y) <= kApothem &&
        std::fabs(kApothem * x + 0.5f * y) <= kApothem &&
        std::fabs(kApothem * x - 0.5f * y) <= kApothem) {
      return glm::vec2(x, y);
    }
  }
}

```

The two separated call sites

```cpp
// The camera: a physical aperture.
const glm::vec2 lens_uv = SampleUnitDisk(rng) * cfg.aperture_radius;

// The diffuse BSDF: nothing here is disk-shaped.
const glm::vec2 d = SampleUnitDisk(rng);
const float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
return d.x * tangent + d.y * binormal + z * normal;
```

Same primitive. The camera calls it directly; the BSDF calls a wrapper whose
entire body is those three lines.

```html <p> Huge break ```
------------------------

## Why do my BSDF and Lens share a Sample Disk

Fair question. If testing my lens can break my diffuse surfaces, why the
coupling?

### BSDF

Because of **Malley's method**. Sample a point inside a unit disk, then project
it along the normal onto a hemisphere originating from the ray's intersection
with the surface. That is how you implement a diffuse surface.

<!-- Sampling_Multidimensional_Functions https://www.pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions
     Figure A.10 -- use the image to show off the method. -->

On first glance the shape of it looks wrong. I am returning a two-dimensional
vector when I have a three-dimensional world. The `z` component is recovered
through the projection, and now we have a 3D vector on the unit sphere.

There are many ways to get a uniform point inside a unit circle. One method is
to sample a unit ball instead of a unit disk, and any direction that lands below
the hemisphere gets pointed back up.

<!-- simple pseudocode or cpp showing normals negatives getting applied -->

I wrote the one I needed and moved on. It works because solid angle projects
onto the disk with a factor of cosine, $dA = \cos\theta \, d\omega$, so a
uniform disk sample lifted to the hemisphere has density

$$
p(\omega) = \frac{\cos\theta}{\pi}
$$

For a Lambertian surface, whose BRDF is $\rho/\pi$, the estimator then cancels
completely:

$$
\frac{f_r \cos\theta}{p(\omega)} = \frac{(\rho/\pi)\cos\theta}{\cos\theta/\pi} = \rho
$$

Each bounce multiplies throughput by exactly the albedo. That is what makes it
fast.

### Lens

Randomizing around the origin allows the ray to focus on a particular plane.
Near and far images blur out, while whatever sits on that plane stays in focus.

<!-- include near and far focus, and not sampling images to compare -->

### Conclusion

Now you see how I ended up sharing them. Both sides have to sample a unit disk,
and voila, for the sake of DRY there is one function. This actually makes so
much sense, you know that right. Apparently pbrt has the same use case and
shares the sampling too. Maybe I am actually a genius "hehe emoji lol"

Notice what that wrapper does _not_ buy me. Having a named
`CosineWeightedHemiSphereSurface` sitting between the BSDF and the disk looks
like the seam that keeps the two uses apart. It isn't. A hexagon goes into the
primitive underneath, and the wrapper inherits it without a word. That is what
hides the bug.

## Any sampler is correct -- if `Pdf()` tells the truth
Now how did I catch it. Yes I can see the multiple call sites, but I could have easily missed it and not realized that my diffuse was off till much later. 
<!-- render images with diffuse where I have either a unit disk or hexagonal disk -->
 Sure I can make teh hexagon but my diffuse might not look that different visually.
 furnace tests did not catch it.

 So I thought what could I check for or test for to ensure the correctness of my diffuse due to the changes for my sampler.


The estimator is unbiased for _any_ way of choosing directions, as long as the
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
which is exactly the furnace test. So one prediction was easy to make before
running anything: the furnace passes.

What I expected to catch it was the other check my notes had named for this job
-- `Pdf()`'s total mass against the fraction of draws landing above the horizon.
A sampler drawing from the wrong shape should show up as a normalisation that nocan 
longer sums to one. That was the theory, anyway.

## I made the change

I swapped a hexagon into `SampleUnitDisk` -- two lines -- and ran every test.

The bokeh, for the record, was lovely.

<!-- IMAGE: docs/images/bokeh-comparison.png (exists). Regenerate both halves
     with ./build/raytracer assets/scenes/bokeh.lua -- once as shipped, once
     with the hexagon rejection test swapped into SampleUnitDisk -- then
     python scripts/make_bokeh_figure.py to stack them. Upload, then replace
     this comment with:
     ![Circular bokeh beside hexagonal bokeh, the same scene rendered twice](URL)
     *Same scene, same seed, one rejection test different. The feature I wanted,
     working on the right. The sharp sphere in that frame is a diffuse surface
     being sampled from the wrong distribution -- and you cannot see it.* -->

<!-- IMAGE: docs/images/malley-hexagon.png (exists). Regenerate it, and every
     number in this post, with the command at the top of
     scripts/malley_figure.cc. Upload, then replace this comment with:
     ![Left: the unit disk and the hexagon a bladed iris puts inside it. Right: the distribution of cos θ after the lift.](URL)
     *Right panel: the disk (blue) follows the exact cosine lobe (grey); the hexagon (orange) doesn't. Its jump at cos θ = 0.5 is the hexagon's apothem.* -->

| Check                               | Compares                   | Real sampler       | Hexagon                        |
| ----------------------------------- | -------------------------- | ------------------ | ------------------------------ |
| Furnace, 10 render tests            | pixels vs environment      | pass               | **pass**                       |
| `Pdf()` mass vs draws above horizon | the total                  | 1.0002 vs 1        | **pass**                       |
| Mean cosine                         | vs 2/3                     | 0.6663             | **0.7437 -- fail**             |
| `cos²/pdf` integral                 | vs 2π/3 ≈ 2.0944           | 2.0943             | **2.3373 -- fail**             |
| Blinn-Phong `cos²/pdf`, 4 angles    | vs 2π/3                    | pass               | **fail at all 4**              |
| Two constructions agree             | Malley vs a second sampler | 0.66689 vs 0.66682 | **0.74395 vs 0.66682 -- fail** |

The top two rows check totals, and both passed on the wrong distribution. The
bottom four check shape, and all four failed. (The figures come from three
harnesses -- the render tests, the BSDF suite and a standalone figure program --
at different seeds and sample counts, so the same quantity drifts in the fourth
decimal from table to table. The gaps that carry the argument are hundreds of
standard errors wide, so none of them turn on that drift.)

**The furnace passed.** All 10 render tests stayed green, and the half-radiance
furnace changed in 8 of 65,536 pixels, by one byte each. `Pdf()` is evaluated on
the direction just drawn, so each bounce's weight is exactly the albedo whatever
the distribution -- and a uniform environment hides the rest.

Strictly, it should have moved _nothing_. Roulette is unbiased and throughput is
exactly 1, so every path that escapes carries back the environment radiance no
matter which way it went. There is only one thing in the renderer that can move a
furnace pixel at all:

```cpp
if (bounces + 1 >= g_max_depth)
  break;  // safety valve, biased
```

A path cut off at eight bounces loses whatever radiance was left, and changing
the aperture shape changes how many paths get that deep. So those 8 pixels are
not the furnace noticing a corrupted sampler. They are a bias I already knew
about, in my bounce limit, leaking through a test that was looking elsewhere. The
energy check's entire response to the bug was eight bytes of an unrelated
problem.

<!-- IMAGE: docs/images/furnace-lambertian-half.png (exists). Regenerate with
     ctest --test-dir build -R furnace, then copy
     tests/out/furnace_lambertian_half.png. Upload, then replace with:
     ![A flat grey square](URL)
     *The furnace with the hexagon in place. Correct looks like this. So does broken.* -->

**The normalisation check passed too** -- the one I had expected to do the
catching. My theory was wrong in a way that is embarrassing in hindsight:
`Pdf()` never changed, so its mass was never going to move, and the lift puts a
hexagon sample above the horizon exactly as faithfully as a disk sample. Both
sides stay at 1. A normalisation check asks whether `Pdf()` is a valid density.
It cannot ask whether it is the density `Sample()` is actually drawing from.

**Blinn-Phong went down with it** at every tested angle, which I had not
predicted at all. Its diffuse half calls the same sampler, so the blast radius
was never one material -- it was every material with a diffuse lobe.

## The test that caught it

Before making the change I'd written a test that builds the cosine lobe a second
way -- the way _Ray Tracing in One Weekend_ does:

```cpp
const glm::vec3 dir = glm::normalize(normal + RandomUnitVector(rng));
```

That route draws from `SampleUnitBall`, not `SampleUnitDisk`, so it can't move
when the aperture does. Two constructions of one distribution have to agree on
every moment, which makes them a **differential test**: no reference renderer,
no known answer needed. Over 2 million samples each:

|                                     | `E[cos]`    | `E[cos²]`   |
| ----------------------------------- | ----------- | ----------- |
| Malley (disk lift)                  | 0.66634     | 0.49964     |
| `normalize(n + RandomUnitVector())` | 0.66662     | 0.49999     |
| hexagon lift                        | **0.74374** | **0.58309** |
| exact                               | 0.66667     | 0.50000     |

"Agree" has to be a number, not a judgement. The test allows four standard
errors of the difference, a tolerance that tightens as samples grow: the real
pair came in 0.00007 apart against 0.00094 allowed. With the hexagon in, they
were 0.077 apart against 0.0008.

It compares two moments, not one, because a single matching number is a weak
fingerprint -- and this exact disk is where I learned that. Two entirely
different integrals over it both come out at 2/3:

$$
\begin{aligned}
\mathbb{E}[r] &= \int_0^1 r \cdot 2r \, dr = \frac{2}{3} \\
\mathbb{E}\!\left[\sqrt{1 - r^{2}}\right] &= \int_0^1 \sqrt{1 - r^{2}} \cdot 2r \, dr = \frac{2}{3}
\end{aligned}
$$

Only the second is `E[cos θ]` -- because `sqrt(1-r²)` _is_ `z` _is_ `cos θ`. I
nearly published the first as proof of Malley's method. A sampler that agreed
with 2/3 on one moment would not have told me which of those two integrals I had
just measured, which is the whole argument for checking a second one.

One limit: two samplers that share a bug agree with each other and are both
wrong -- these two draw from the same random number generator. That's why the
closed-form checks stay in the suite. The differential test says the two routes
match; the closed forms say they match the right answer.

## Mechanism versus coincidence

The two routes start from sibling primitives. `SampleUnitDisk` and
`SampleUnitBall` are the same **mechanism** one dimension apart: sample the
enclosing box, reject what lands outside the shape. What you get comes from the
map you apply afterwards:

| Primitive        | Then                                 | Gives             | Used by                 |
| ---------------- | ------------------------------------ | ----------------- | ----------------------- |
| `SampleUnitDisk` | nothing                              | uniform disk      | camera aperture         |
| `SampleUnitDisk` | lift `z = √(1−r²)`                   | cosine hemisphere | Lambertian, Blinn-Phong |
| `SampleUnitBall` | normalize                            | uniform sphere    | rough-metal fuzz        |
| `SampleUnitBall` | normalize, add the normal, normalize | cosine hemisphere | the differential test   |

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

- **Share on mechanism, never on coincidence.** And don't mistake a named
  wrapper for protection -- I had one, and the hexagon went straight through it
  into the primitive underneath. What actually helps is a comment at the shared
  function saying why it's shared, and giving the lens its own sampler _before_
  anyone reshapes it.
- **Test the shape, not just the total.** The furnace and the normalisation check
  both passed, and both measure totals. Neither one can tell two distributions
  apart when they carry the same mass.
- **Build the bug on purpose.** Writing down which tests I expected to fire cost
  nothing and was wrong twice over: the check I trusted stayed green, and a
  material I hadn't considered broke. Two lines and one test run is a cheap way
  to find out what your suite is really watching -- much cheaper than finding out
  from a render six months from now.

---

_From a path tracer I'm building in C++. Next: getting total internal reflection
wrong in four different ways before getting it right._
