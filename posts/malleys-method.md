---
title: "A Hexagonal Aperture Would Break Every Diffuse Surface in My Renderer"
published: false
description: My camera's lens sampler and my diffuse BSDF sampler depend on the same function. A bladed iris would corrupt every diffuse bounce.
tags: graphics, cpp, testing, math
---

<!-- COVER: hexagonal bokeh
     render this aperture belongs to. Upload it and paste the URL into
     cover_image: above. dev.to crops covers to 1000x420. -->

I have experimented with a lot of features on my raytracing journey. I played with reflections, implemented shadows, and lost braincells getting sceens to render. Finally, I implemented defocus blur.

![Three spheres receding from the camera. The near red one is sharp; the middle green and far blue ones are soft.](../docs/images/step5-rack-focus-near.png)

<!-- IMAGE: the link above is a repo path, for local preview only. Regenerate
     with ./build/raytracer assets/scenes/thin_lens.lua, then copy
     renders/thin_lens_near.png. Swap the path for an uploaded URL before
     publishing. -->

---

The image reminded me of bokeh we see in photo and sometimes they aren't round but have other shapes

![A photograph of out-of-focus lights, each one blurred into a heart shape rather than a circle.](../docs/images/reference/heart-bokeh.jpg)

_Not a render. A heart cut out of card, taped over a camera lens_

<!-- IMAGE: reference photo, not mine. Credit the source before publishing, and
     swap the repo path for an uploaded URL. -->

So I tried it with what I already had, and this is what came out.

<!-- Basic image render with circular bokeh -->

![Two renders side by side, labelled Disk aperture and Hexagonal aperture: the same scattered out-of-focus highlights, round on the left and six-sided on the right.](../docs/images/bokeh-comparison.png)

_Same scene, same seed. The only difference is the shape of the lens sampler_

<!-- IMAGE: docs/images/bokeh-comparison.png (exists). Regenerate both halves
     with ./build/raytracer assets/scenes/bokeh.lua -- once as shipped, once
     with the hexagon rejection test swapped into SampleUnitDisk -- then
     python scripts/make_bokeh_figure.py to stack them. Swap the repo path for
     an uploaded URL before publishing. -->

I did 2 passes, one circular and another hexagonal.

I felt like I could make anything a bokeh "excited emoji", why not try harder but I noticed an issue before I could proceed.

Every diffuse surface in my renderer would suffer for that one change.

---

The primitive function used. utilizes rejection sampling in uniit disk to get a circular shape

```cpp

inline glm::vec2 SampleUnitDisk(Rng& rng) {
  while(true){
    float x = rng.Range(-1.0f, 1.0f);
    float y = rng.Range(-1.0f, 1.0f);
    if (x * x + y * y <= 1.0f) return glm::vec2(x, y);
  }
}

```

what the hexagonal change looked like:

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

The two separate call sites

```cpp
// The camera: a physical aperture.
const glm::vec2 lens_uv = SampleUnitDisk(rng) * cfg.aperture_radius;

// The diffuse BSDF wrapper
inline glm::vec3 CosineWeightedHemiSphereSurface(Rng& rng,
                                                 const glm::vec3& normal,
                                                 const glm::vec3& tangent,
                                                 const glm::vec3& binormal) {
  const glm::vec2 d = SampleUnitDisk(rng);
  const float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
  return d.x * tangent + d.y * binormal + z * normal;
}
```

Same primitive. The camera calls it directly while the BSDF calls a wrapper.

---

## Why do my BSDF and Lens share a Sample Disk?

Fair question. If testing, or more fittingly, playing with my lens can break my diffuse surfaces, why the coupling?

### For BSDF

---

Because of **Malley's method**. Sample a point inside a unit disk, then project it along the normal onto a hemisphere originating from the ray's intersection with the surface. That is how you implement a diffuse surface, and apparently it is wicked fast.

![Points scattered on a flat disk, each joined by a vertical line to the point directly above it on a hemisphere resting on the same plane.](../docs/images/reference/pbrt-malley-figure-a10.png)

_The lift, drawn. Every point on the disk rises straight up to the dome above
it. Figure A.10 from [Physically Based Rendering: From Theory to
Implementation](https://www.pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions),
4th ed., by Matt Pharr, Wenzel Jakob and Greg Humphreys._

<!-- IMAGE: reference figure, not mine -- copied from pbr-book.org, credited in
     the caption above. Confirm reuse is acceptable before publishing. Drawing
     a replacement is not free: malley_figure.cc rasterises flat 2D panels and
     has no perspective projection, so this view would be new code.
     Swap the repo path for an uploaded URL. -->

### For Lens

Randomizing around the origin allows the ray to focus on a particular plane.
Near and far images blur out, while whatever sits on that plane stays in focus. Guess how we randomize around the origin.
Exactly!!
We sample within a unit disk.

![Hand-drawn diagram: three rays leave three separate points on a lens and converge on a single point of a virtual film plane standing at the focus plane.](../docs/images/reference/rtiow-fig-1.22-cam-film-plane.jpg)

_"Camera focus
plane", Figure 1.22 of
[Ray Tracing in One
Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html#defocusblur)
by Peter Shirley, Trevor David Black and Steve Hollasch, released under CC0._

<!-- IMAGE: docs/images/reference/rtiow-fig-1.22-cam-film-plane.jpg, the
     original from the RayTracing/raytracing.github.io repo (CC0-1.0, so reuse
     is unrestricted; the credit above is courtesy, not obligation). Swap the
     repo path for an uploaded URL before publishing. -->

<!-- include near and far focus, and not sampling images to compare -->

### Conclusion

Now you see how I ended up sharing them. Both sides have to sample a unit disk and voila. Apparently PBRT implements it the same way. I guess great minds do think alike. "hehe emoji lol"

## Any sampler is correct -- if `Pdf()` tells the truth

So how did I catch it? I read the code, because I had just finished writing it,
and I saw the two call sites sitting there. That is not a catch I can rely on. I
could easily have missed it, someone opening the file a year from now could miss
it too, and neither of us would know the diffuse was off until much later.

Looking at the render does not help either. I can make the hexagon, sure, but
the diffuse barely moves.

![Two renders of the same all-diffuse Cornell box side by side, labelled Disk sampler and Hexagon sampler, and indistinguishable by eye.](../docs/images/diffuse-comparison.png)

_Every surface in that box is Lambertian, so there is nothing else to look at._

Do you notice the difference? Do you see the shadows shift? Yeah, neither do I.
Without the two frames side by side I would have walked straight past this.

<!-- IMAGE: docs/images/diffuse-comparison.png. Both halves come from
     assets/scenes/diffuse_sampler.lua at 256 spp -- once as shipped, once with
     the hexagon rejection test in SampleUnitDisk -- then
     python scripts/make_diffuse_figure.py to stack them and print the numbers
     above. The noise floor is a third render, renders/diffuse_disk_seed2.png:
     the shipped sampler with the seed constant in renderer.cc perturbed, which
     is noise of the same size with no bias. Swap the repo path for an uploaded
     URL before publishing. -->

So I said to myself: what can I actually check for?

Not the code. Reading it is how I found this one, and reading it is exactly the
catch I just said I could not rely on. What needs checking is what the sampler
_draws_ -- the shape of it, not the lines that produce it.

So start with the shape mine draws, and why it works at all. I wrote it the way
I did because it flowed so naturally with Malley's method -- did not think about
it, moved on. It works because solid angle projects onto the disk with a factor
of cosine, $dA = \cos\theta \, d\omega$, so a uniform disk sample lifted to the
hemisphere has density

$$
p(\omega) = \frac{\cos\theta}{\pi}
$$

For a Lambertian surface, whose BRDF is $\rho/\pi$, the estimator then cancels
completely:

$$
\frac{f_r \cos\theta}{p(\omega)} = \frac{(\rho/\pi)\cos\theta}{\cos\theta/\pi} = \rho
$$

Each bounce multiplies throughput by exactly the albedo, `ρ` -- the fraction of
light a surface sends back, somewhere between 0 and 1. No trig, no integral,
nothing left to evaluate. That is where the wicked fast comes from.

Read that derivation again, though, and notice what it never asks for. It never
asks for a disk.

Which means I can build the same lobe without one. Here is the road _Ray Tracing
in One Weekend_ takes: stand at the tip of the normal, push out by a random unit
vector, and normalise where you land.

```cpp
const glm::vec3 dir = glm::normalize(normal + RandomUnitVector(rng));
```

That one draws from a ball. It never touches a disk, and it arrives at the same
cosine lobe anyway. Two roads, one distribution -- and that is something I can
check without knowing in advance where the distribution is supposed to be. Hold
onto it.

### The lift, and what the hexagon does to it

Malley's method is one picture. Put the shape underneath the hemisphere like a
floor plan, and let every point on it rise straight up to the dome. The height
it lands at *is* cos θ. Nobody added the cosine; the geometry handed it over.
The cloud of directions you end up with is the **lobe** -- a fat dome, widest
straight up, thinned out at grazing angles.

![Two oblique drawings of a hemisphere. Under the left one a disk, under the right a hexagon, with vertical lines lifting floor points up onto the dome. The disk's rim follows the horizon; the hexagon's rises into six arches.](../docs/images/malley-lift.png)

*Same dome, same straight-up lift, same horizon, and in both the rise is cos θ.
The only thing that differs is the rim. The disk's sits on the horizon the whole
way round; the hexagon's plunges there at six corners and stops dead at cos θ =
0.5 in between.*

<!-- IMAGE: docs/images/malley-lift.png. Regenerate with
     python scripts/malley_lift_figure.py (needs matplotlib). Swap the repo
     path for an uploaded URL before publishing. -->

So swap the hexagon in and look at how little moves. Same dome. Same lift. Every
point still lands above the horizon, because the hexagon sits inside the circle
and `1 - r²` never goes negative. The centre still maps to the normal. It is
still a fat lobe crowding around it.

What moves is the rim, and only the rim. A hexagon's corners reach r = 1, but
its edges cut in to the apothem, √3/2 ≈ 0.866 -- and 0.866 lifts to cos θ = 0.5.
So the grazing directions survive in six slivers and are missing the rest of the
way round.

Which is enough to guess the damage before running anything. The hexagon is
inscribed, so it is strictly smaller -- 3√3/2 against π, or 82.7% of the disk.
The 17.3% it gives up is all rim, all of it outside r = 0.866, so every direction
in it lifts to a cos θ between 0 and 0.5. Call that 0.25 on average, take it out
of the disk's 2/3, and rebalance over what is left:

$$
\mathbb{E}_{\text{hex}}[\cos\theta] \;\approx\; \frac{2/3 - 0.173 \times 0.25}{0.827} \;\approx\; 0.75
$$

Cut the flattest directions out of a crowd and the average has to tilt up. The
only question was how far. Measured: 0.744 -- and the grazing traffic really is
gone, a quarter of a disk's bounces go out past 60° from the normal against the
hexagon's 9%. Remember 0.744.

### None of that is the bug

The shape of the lobe was never the thing that had to be right.

Picking directions is polling a crowd. You cannot ask every direction, so you
ask a few thousand and average what they say. If you ask some directions more
often than others, their answers have to count for less, or the loud ones win
the poll. `Pdf()` is exactly that correction: how likely I was to ask this
direction. Divide by it and the over-asked get scaled back down.

So the floor plan is mine to choose. Uniform over the hemisphere works. A
hexagonal lobe works too, as long as its `Pdf()` describes a hexagon. The shape
decides how fast the noise clears, never what the image converges to. Cosine
wins on speed and on that cancellation, not on correctness.

The hexagon isn't a bug because it's a hexagon. It's a bug because `Sample()`
started drawing from one while `Pdf()` kept quoting circle prices. I poll the
hexagon crowd and weight every answer as though I had polled the disk.

Which is what makes that second road worth keeping. It is not a better lobe --
it is the same lobe, reached without a disk. So the day the disk stops being a
disk, the two stop agreeing, and they say so without anyone needing to know the
right answer first.

### What it converges to instead

Nothing in the code so much as flinches. `Pdf()` is evaluated on the
direction just drawn, so the cosines still cancel and each bounce still
multiplies throughput by exactly ρ. The weights are fine. It is the crowd that
moved. Drawing from a lobe `q` while `Pdf()` reports the cosine converges to

$$
\rho \cdot \mathbb{E}_q[L_{\text{in}}]
\quad\text{instead of}\quad
\rho \cdot \mathbb{E}_{\cos}[L_{\text{in}}]
$$

Left to right: ρ is the albedo again, the fraction that survives the bounce.
$L_{\text{in}}$ is **radiance** -- the brightness arriving along one single
direction, what one ray carries. $\mathbb{E}$ is just an average, and the
subscript names the crowd that got polled: `q` is the lobe I actually drew from,
`cos` is the lobe `Pdf()` still believes in.

Only one of those two averages is a physical quantity. Average radiance over the
cosine lobe and you get the **irradiance**, the total light landing on that
point, divided by π:

$$
\mathbb{E}_{\cos}[L_{\text{in}}]
= \frac{1}{\pi}\int_{\Omega} L_{\text{in}}\cos\theta \, d\omega
= \frac{E}{\pi}
$$

Radiance is per direction, one ray's worth. Irradiance is what the surface
collects from all of them at once, and the cosine is the exchange rate between
them: light arriving flat across the surface contributes less than light
straight overhead. Sampling the cosine lobe is how you get the irradiance
without ever doing that integral -- the lobe does the weighting for you, by
crowding.

Lift the hexagon and I am still averaging radiance -- a perfectly good average,
over a crowd that no longer weights by the cosine. It is not the irradiance of
anything.

Unless every direction says the same thing. Poll the hexagon, poll the disk,
poll at random -- when the light arriving is identical no matter where you look,
every average comes back identical too, and a rigged poll still lands on the
right answer. That is the furnace test exactly: one uniform environment, the
same radiance from every direction. So one prediction was free before running
anything. The furnace passes.

What I expected to catch it was the other check my notes had named for this job
-- `Pdf()`'s total mass against the fraction of draws landing above the horizon.
A sampler drawing from the wrong shape should show up as a normalisation that no
longer sums to one. That was the theory, anyway.

## I tested my hypothesis

I reverted to a hexagon in `SampleUnitDisk` -- two lines -- and ran every test.

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
furnace changed in 9 of 65,536 pixels, by one byte each. `Pdf()` is evaluated on
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
the aperture shape changes how many paths get that deep. So those 9 pixels are
not the furnace noticing a corrupted sampler. They are a bias I already knew
about, in my bounce limit, leaking through a test that was looking elsewhere. The
energy check's entire response to the bug was nine bytes of an unrelated
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

<!-- Section for random bokehs and shapes just because I can -->

including stars, heights, kh crown -->
