// A constant-density medium, tested without a render.
//
// One claim, Beer-Lambert: the fraction of rays crossing a thickness d of
// medium without stopping is e^(-density * d). The boundary is a sphere and
// every ray runs through its centre, so d is the diameter, or the radius for
// a ray that starts at the centre.
//
// Rays are deliberately not unit length. A medium that measures depth in t
// instead of in distance passes with unit rays and fails here.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <memory>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/primitive.h"
#include "render/sampling.h"
#include "support/statistics.h"

namespace {

constexpr double kDensity = 0.7;
constexpr double kRadius = 1.5;
constexpr int kRays = 200000;

ConstantMedium Smoke() {
  return ConstantMedium(
      std::make_unique<NonhierSphere>(glm::vec3(0.0f), kRadius), kDensity);
}

// The fraction of rays from `origin` along `dir` that pass through.
stats::Estimate Transmitted(const glm::vec3& origin, const glm::vec3& dir,
                            uint32_t seed) {
  ConstantMedium smoke = Smoke();
  Rng rng(seed);
  stats::Estimate passed;
  for (int i = 0; i < kRays; ++i) {
    Ray ray(origin, dir, 0.0f, &rng);
    HitRecord hit;
    passed.Add(smoke.IsHit(ray, 1e-4f, 1e6f, hit) ? 0.0 : 1.0);
  }
  return passed;
}

}  // namespace

TEST_SUITE("medium/transmission") {
  TEST_CASE("medium: the fraction passing through is e^(-density d)") {
    const double through = std::exp(-kDensity * 2.0 * kRadius);
    // Half-length and double-length rays cross the same distance.
    for (float scale : {0.5f, 1.0f, 2.0f}) {
      CAPTURE(scale);
      CHECK_ESTIMATE(Transmitted(glm::vec3(-5.0f, 0.0f, 0.0f),
                                 glm::vec3(scale, 0.0f, 0.0f), 3u),
                     through);
    }
  }

  TEST_CASE("medium: a ray starting inside crosses only what is ahead") {
    CHECK_ESTIMATE(
        Transmitted(glm::vec3(0.0f), glm::vec3(0.0f, 0.5f, 0.0f), 5u),
        std::exp(-kDensity * kRadius));
  }

  TEST_CASE(
      "medium: a stop lies inside and a miss of the boundary never stops") {
    ConstantMedium smoke = Smoke();
    Rng rng(7u);
    for (int i = 0; i < 1000; ++i) {
      Ray through(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                  0.0f, &rng);
      HitRecord hit;
      if (smoke.IsHit(through, 1e-4f, 1e6f, hit)) {
        CHECK(glm::length(hit.GetHitPoint()) <= kRadius + 1e-4);
        CHECK(hit.GetT() >= 5.0f - kRadius - 1e-4f);
      }

      Ray beside(glm::vec3(-5.0f, 2.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                 0.0f, &rng);
      CHECK_FALSE(smoke.IsHit(beside, 1e-4f, 1e6f, hit));
    }
  }

}  // TEST_SUITE medium/transmission
