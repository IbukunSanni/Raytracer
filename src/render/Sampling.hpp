// Raytracer -- sampling utilities
//
//   1. Per-thread RNG so renders are reproducible and lock-free.
//   2. sampleUnitDisk() -- the aperture shape for depth of field, and,
//      lifted to the hemisphere by Malley's method, the whole body of
//      cosine-weighted BSDF sampling. One primitive, two consumers.
//   3. sampleUnitBall() -- the same rejection trick one dimension up, and
//      randomUnitVector() on top of it: the fuzz lobe of a rough metal.

#pragma once

#include <glm/glm.hpp>
#include <random>

constexpr float kPI = 3.14159265358979323846f;

// One small number, shared by everything that needs a "close enough to
// zero" cutoff: the renderer's self-intersection offset and ray tMin, and
// the guard that keeps a degenerate sample out of glm::normalize().
constexpr float kEpsilon = 0.000001f;

// Per-thread RNG. Each thread seeds from its own index, so a render is
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

// Uniform point in the unit disk (x^2 + y^2 <= 1) by rejection sampling.
// This is the aperture shape, so it is also the shape of the bokeh.
inline glm::vec2 sampleUnitDisk(Rng & rng)
{
	const int maxAttempts = 10;

	for (int i = 0; i < maxAttempts; ++i) {
		float x = rng.range(-1.0f, 1.0f);
		float y = rng.range(-1.0f, 1.0f);
		if (x * x + y * y <= 1.0f)
			return glm::vec2(x, y);
	}

	return glm::vec2(0.0f, 0.0f);
}

// Uniform point in the unit ball (x^2 + y^2 + z^2 <= 1) by rejection
// sampling -- the same technique as sampleUnitDisk, one dimension up.
//
// The ball fills only pi/6 of the cube, so a draw fails almost half the
// time and the attempt cap has to sit far above the disk's to keep the
// fallback direction from showing up as a bias: at 64 attempts it is
// reached about once in every 10^20 calls.
inline glm::vec3 sampleUnitBall(Rng & rng)
{
	const int maxAttempts = 64;

	for (int i = 0; i < maxAttempts; ++i) {
		const float x = rng.range(-1.0f, 1.0f);
		const float y = rng.range(-1.0f, 1.0f);
		const float z = rng.range(-1.0f, 1.0f);
		const float lengthSq = x * x + y * y + z * z;

		// The lower bound keeps randomUnitVector() from normalising a
		// point sitting on the origin.
		if (lengthSq <= 1.0f && lengthSq >= kEpsilon)
			return glm::vec3(x, y, z);
	}

	return glm::vec3(1.0f, 0.0f, 0.0f);
}

// A uniformly distributed direction on the unit sphere.
inline glm::vec3 randomUnitVector(Rng & rng)
{
	return glm::normalize(sampleUnitBall(rng));
}

inline void createOrthoNormalBasis(const glm::vec3 & n,
                                   glm::vec3 * tangent,
                                   glm::vec3 * binormal)
{
	const glm::vec3 seed = (std::fabs(n.x) > 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
	                                               : glm::vec3(1.0f, 0.0f, 0.0f);
	*tangent  = glm::normalize(glm::cross(seed, n));
	*binormal = glm::cross(n, *tangent);
}

inline glm::vec3 cosineWeightedHemiSphereSurface(Rng & rng,
                                                 const glm::vec3 & normal,
                                                 const glm::vec3 & tangent,
                                                 const glm::vec3 & binormal){
	const glm::vec2 d = sampleUnitDisk(rng);
	const float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
	return d.x * tangent + d.y * binormal + z * normal;
}
