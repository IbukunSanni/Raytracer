// A4 -- sampling utilities
//
// Two jobs here:
//   1. Give every render thread its own random number generator.
//      The old rand_float() used rand(), which shares one global state
//      across threads -- that is both a correctness problem (the
//      sequence is not reproducible) and a contention problem (glibc
//      takes a lock on every call).
//   2. Provide the lens-sampling primitive that depth of field needs.

#pragma once

#include <glm/glm.hpp>
#include <random>

// ---------------------------------------------------------------------
// Per-thread RNG.  Each thread seeds from its own index, so a render is
// reproducible: same scene + same thread count => same image.
class Rng {
public:
	explicit Rng(uint32_t seed) : m_engine(seed), m_dist(0.0f, 1.0f) {}

	// Uniform in [0, 1)
	float next() { return m_dist(m_engine); }

	// Uniform in [lo, hi)
	float range(float lo, float hi) { return lo + (hi - lo) * next(); }

private:
	std::mt19937 m_engine;
	std::uniform_real_distribution<float> m_dist;
};

// ---------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Return a point sampled UNIFORMLY from the unit disk: the set of
// (x, y) with x^2 + y^2 <= 1.  This is the shape of a camera aperture,
// so its distribution is literally the shape of your bokeh.
//
// Two standard ways:
//
//   (a) Rejection sampling.  Draw x and y uniformly from [-1, 1] and
//       throw the pair away if x^2 + y^2 > 1.  Loop until one lands
//       inside.  Simple and exactly uniform.  Expect ~1.27 draws per
//       accepted sample (4/pi).
//
//   (b) Polar with a square root.  theta = 2*pi*u1, r = sqrt(u2).
//       The sqrt is the whole trick: without it you get r = u2, which
//       piles samples up at the centre, because the area of an annulus
//       grows with r.  Always exactly two random numbers, no loop.
//
// What the OLD code did wrong, for contrast: it drew x and y from
// [0, 1) -- never negative -- and rejected on length >= 1.  That is a
// QUARTER disk in the +x+y quadrant.  Subtracting 0.5 afterwards
// re-centred it but did not make it round, so the aperture was a
// lopsided wedge rather than a disk.
//
// While this returns vec2(0, 0) the lens has zero radius, which is
// exactly a pinhole camera -- so the renderer stays correct and simply
// produces no blur.  Implement it and the blur appears.
inline glm::vec2 sampleUnitDisk(Rng & rng)
{
	// TODO: replace this with (a) or (b) above.
	(void) rng;
	return glm::vec2(0.0f, 0.0f);
}
