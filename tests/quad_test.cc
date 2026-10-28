// A quad's hit, tested without a render.
//
// Two claims. A ray lands where its (alpha, beta) say: the hit is at that
// point and reports it as (u, v), and a point just past any edge, or a ray
// beside the quad and parallel to it, misses. And the normal faces the ray,
// from either side.
//
// The quad is skewed and tilted: its edges differ in length, meet at no
// right angle, and lie in no axis plane. Swapping alpha and beta, or scaling
// w by anything but 1 / |cross(u, v)|^2, moves every (u, v) checked here.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/primitive.h"

namespace {

const glm::vec3 kQ(1.0f, -0.5f, 0.25f);
const glm::vec3 kU(2.0f, 0.0f, 0.5f);
const glm::vec3 kV(0.5f, 1.5f, -0.25f);

glm::vec3 PointAt(float alpha, float beta) {
  return kQ + alpha * kU + beta * kV;
}

glm::vec3 Normal() { return glm::normalize(glm::cross(kU, kV)); }

// A slanted ray at the plane point (alpha, beta), from the side `side` (+1
// or -1) of the normal. It reaches the point at t = 1.
Ray RayAt(float alpha, float beta, float side) {
  const glm::vec3 p = PointAt(alpha, beta);
  const glm::vec3 origin = p + side * (Normal() + glm::vec3(0.3f, -0.2f, 0.1f));
  return Ray(origin, p - origin, 0.0f);
}

}  // namespace

TEST_SUITE("quad/hit") {
  TEST_CASE("quad hit: lands at its (alpha, beta), and misses past each edge") {
    Quad quad(kQ, kU, kV);
    const float inside[][2] = {{0.2f, 0.7f}, {0.9f, 0.15f}, {0.03f, 0.96f}};
    for (const auto& ab : inside) {
      CAPTURE(ab[0]);
      CAPTURE(ab[1]);
      Ray ray = RayAt(ab[0], ab[1], 1.0f);
      HitRecord hit;
      REQUIRE(quad.IsHit(ray, 1e-4f, 10.0f, hit));
      CHECK(hit.GetT() == doctest::Approx(1.0f).epsilon(1e-5));
      const glm::vec3 p = PointAt(ab[0], ab[1]);
      CHECK(glm::length(hit.GetHitPoint() - p) < 1e-5f);
      CHECK(hit.GetU() == doctest::Approx(ab[0]).epsilon(1e-5));
      CHECK(hit.GetV() == doctest::Approx(ab[1]).epsilon(1e-5));
    }

    const float outside[][2] = {
        {1.02f, 0.5f}, {-0.02f, 0.5f}, {0.5f, 1.02f}, {0.5f, -0.02f}};
    for (const auto& ab : outside) {
      CAPTURE(ab[0]);
      CAPTURE(ab[1]);
      Ray ray = RayAt(ab[0], ab[1], 1.0f);
      HitRecord hit;
      CHECK_FALSE(quad.IsHit(ray, 1e-4f, 10.0f, hit));
    }

    Ray beside(PointAt(0.5f, 0.5f) + 0.5f * Normal(), kU, 0.0f);
    HitRecord hit;
    CHECK_FALSE(quad.IsHit(beside, 1e-4f, 10.0f, hit));
  }

  TEST_CASE("quad hit: the normal faces the ray from either side") {
    Quad quad(kQ, kU, kV);
    for (float side : {1.0f, -1.0f}) {
      CAPTURE(side);
      Ray ray = RayAt(0.4f, 0.6f, side);
      HitRecord hit;
      REQUIRE(quad.IsHit(ray, 1e-4f, 10.0f, hit));
      const glm::vec3 n = glm::normalize(hit.GetNormal());
      CHECK(glm::dot(n, ray.GetDirection()) < 0.0f);
      CHECK(std::abs(glm::dot(n, Normal())) == doctest::Approx(1.0f));
    }
  }
}
