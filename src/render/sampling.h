// Raytracer -- sampling utilities

#ifndef RAYTRACER_SRC_RENDER_SAMPLING_H_
#define RAYTRACER_SRC_RENDER_SAMPLING_H_

#include <glm/glm.hpp>
#include <random>

constexpr float kPI = 3.14159265358979323846f;

// One small number, shared by everything that needs a "close enough to
// zero" cutoff
constexpr float kEpsilon = 0.000001f;

// Per-thread RNG.
class Rng {
 public:
  explicit Rng(uint32_t seed) : engine_(seed) {}

  float Next() {
    constexpr float kInv2Pow24 = 1.0f / 16777216.0f;  // 2^-24, exact
    return static_cast<float>(engine_() >> 8) * kInv2Pow24;
  }

  // Uniform in [lo, hi)
  float Range(float lo, float hi) { return lo + (hi - lo) * Next(); }

 private:
  std::mt19937 engine_;
};

// Uniform point in the unit disk (x^2 + y^2 <= 1), by inverting the radial
// CDF
inline glm::vec2 SampleUnitDisk(Rng& rng) {
  const float phi = 2.0f * kPI * rng.Next();
  const float r = std::sqrt(rng.Next());
  return glm::vec2(r * std::cos(phi), r * std::sin(phi));
}

// A uniformly distributed direction on the unit sphere.
inline glm::vec3 RandomUnitVector(Rng& rng) {
  float z = 1.0f - 2.0f * rng.Next();
  float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
  float phi = 2.0f * kPI * rng.Next();
  float x = r * std::cos(phi);
  float y = r * std::sin(phi);
  return glm::vec3(x, y, z);
}

// Uniform point in the unit ball (x^2 + y^2 + z^2 <= 1): a uniform direction
// scaled by cbrt(u), since the volume inside radius R grows as R^3. Always
// three draws, no loop.
inline glm::vec3 SampleUnitSphere(Rng& rng) {
  const float radius = std::cbrt(1.0f - rng.Next());
  return radius * RandomUnitVector(rng);
}

// TODO: wrap tangent, binormal and normal in a small Onb struct (u, v, w)
// with a local(a, b, c) that turns local coordinates into world ones, as the
// book's ONB does. Lambertian and Blinn-Phong sampling could then call one
// helper in place of combining the three vectors by hand.
inline void CreateOrthoNormalBasis(const glm::vec3& n, glm::vec3* tangent,
                                   glm::vec3* binormal) {
  const glm::vec3 seed = (std::fabs(n.x) > 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                 : glm::vec3(1.0f, 0.0f, 0.0f);
  *tangent = glm::normalize(glm::cross(seed, n));
  *binormal = glm::cross(n, *tangent);
}

inline glm::vec3 CosineWeightedHemiSphereSurface(Rng& rng,
                                                 const glm::vec3& normal,
                                                 const glm::vec3& tangent,
                                                 const glm::vec3& binormal) {
  const glm::vec2 d = SampleUnitDisk(rng);
  const float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
  return d.x * tangent + d.y * binormal + z * normal;
}

#endif  // RAYTRACER_SRC_RENDER_SAMPLING_H_
