// Raytracer -- sampling utilities
//
//   1. Per-thread RNG so renders are reproducible and lock-free.
//   2. sampleUnitDisk() -- the lens primitive depth of field needs.

#pragma once

#include <glm/glm.hpp>
#include <random>

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
