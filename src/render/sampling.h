// Raytracer -- sampling utilities
//
//   1. Per-thread RNG so renders are reproducible and lock-free.
//   2. SampleUnitDisk() -- the aperture shape for depth of field, and,
//      lifted to the hemisphere by Malley's method, the whole body of
//      cosine-weighted BSDF sampling. One primitive, two consumers.
//   3. SampleUnitBall() -- the same rejection trick one dimension up, and
//      RandomUnitVector() on top of it: the fuzz lobe of a rough metal.

#ifndef RAYTRACER_SRC_RENDER_SAMPLING_H_
#define RAYTRACER_SRC_RENDER_SAMPLING_H_

#include <glm/glm.hpp>
#include <random>

constexpr float kPI = 3.14159265358979323846f;

// One small number, shared by everything that needs a "close enough to
// zero" cutoff
constexpr float kEpsilon = 0.000001f;

// Per-thread RNG. Each thread seeds from its own index, so a render is
// reproducible: same scene + same thread count => same image.
class Rng {
 public:
  explicit Rng(uint32_t seed) : engine_(seed), dist_(0.0f, 1.0f) {}

  // Uniform in [0, 1)
  float Next() { return dist_(engine_); }

  // Uniform in [lo, hi)
  float Range(float lo, float hi) { return lo + (hi - lo) * Next(); }

 private:
  std::mt19937 engine_;
  std::uniform_real_distribution<float> dist_;
};

// Uniform point in the unit disk (x^2 + y^2 <= 1) by rejection sampling.
// This is the aperture shape, so it is also the shape of the bokeh.
inline glm::vec2 SampleUnitDisk(Rng& rng) {
  const int max_attempts = 10;

  for (int i = 0; i < max_attempts; ++i) {
    float x = rng.Range(-1.0f, 1.0f);
    float y = rng.Range(-1.0f, 1.0f);
    if (x * x + y * y <= 1.0f) return glm::vec2(x, y);
  }

  return glm::vec2(0.0f, 0.0f);
}

// Uniform point in the unit ball (x^2 + y^2 + z^2 <= 1) by rejection
// sampling
//
// The ball fills only pi/6 of the cube, so a draw fails almost half the
// time and the attempt cap has to sit far above the disk's to keep the
// fallback direction from showing up as a bias: at 64 attempts it is
// reached about once in every 10^20 calls.
inline glm::vec3 SampleUnitBall(Rng& rng) {
  const int max_attempts = 64;

  for (int i = 0; i < max_attempts; ++i) {
    const float x = rng.Range(-1.0f, 1.0f);
    const float y = rng.Range(-1.0f, 1.0f);
    const float z = rng.Range(-1.0f, 1.0f);
    const float length_sq = x * x + y * y + z * z;

    // The lower bound keeps RandomUnitVector() from normalising a
    // point sitting on the origin.
    if (length_sq <= 1.0f && length_sq >= kEpsilon) return glm::vec3(x, y, z);
  }

  return glm::vec3(1.0f, 0.0f, 0.0f);
}

// A uniformly distributed direction on the unit sphere.
inline glm::vec3 RandomUnitVector(Rng& rng) {
  return glm::normalize(SampleUnitBall(rng));
}

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
