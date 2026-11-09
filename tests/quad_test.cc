// A quad's hit, tested without a render.
//
// Three claims. A ray lands where its (alpha, beta) say: the hit is at that
// point and reports it as (u, v), and a point just past any edge, or a ray
// beside the quad and parallel to it, misses. The normal faces the ray, from
// either side. And SampleToward() and Pdf() describe the same distribution.
//
// The quad is skewed and tilted: its edges differ in length, meet at no
// right angle, and lie in no axis plane. Swapping alpha and beta, or scaling
// w by anything but 1 / |cross(u, v)|^2, moves every (u, v) checked here.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/primitive.h"
#include "render/sampling.h"
#include "support/statistics.h"

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
  return Ray(origin, p - origin, 0.0f, nullptr);
}

// Samples per estimate: enough that a wrong factor in the density, or a
// sampler uneven by a percent, lands well outside four standard errors.
constexpr int kSamples = 2000000;

// A function of direction that no density here is proportional to, so a
// matched pair of errors in SampleToward() and Pdf() has nothing to cancel.
double TestFunction(const glm::vec3& w) {
  const double lean = glm::dot(w, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
  return lean * lean;
}

// Three measurements of a quad as seen from one point. The sky_ ones draw
// directions uniformly over the sphere and never call SampleToward(), so the
// sampled_ ones have something independent to agree with.
struct QuadSampling {
  stats::Estimate pdf_mass;       // integral of Pdf() over the sphere
  stats::Estimate sampled_angle;  // mean of 1 / pdf over SampleToward()
  stats::Estimate sky_angle;      // fraction of the sphere covered, as sr
  stats::Estimate sampled_f;      // mean of f / pdf over SampleToward()
  stats::Estimate sky_f;          // integral of f over the covered directions
  int missed = 0;                 // SampleToward() draws Pdf() calls a miss
};

QuadSampling MeasureSampling(const Quad& quad, const glm::vec3& origin,
                             uint32_t seed) {
  Rng rng(seed);
  QuadSampling m;
  const double sphere = 4.0 * kPI;
  for (int i = 0; i < kSamples; ++i) {
    const glm::vec3 w = RandomUnitVector(rng);
    const double pdf_w = quad.Pdf(origin, w);
    const bool covered = pdf_w > 0.0;
    m.pdf_mass.Add(pdf_w * sphere);
    m.sky_angle.Add(covered ? sphere : 0.0);
    m.sky_f.Add(covered ? TestFunction(w) * sphere : 0.0);

    const glm::vec3 out = quad.SampleToward(origin, rng);
    const double pdf = quad.Pdf(origin, out);
    if (pdf <= 0.0) {
      ++m.missed;
      m.sampled_angle.Add(0.0);
      m.sampled_f.Add(0.0);
      continue;
    }
    m.sampled_angle.Add(1.0 / pdf);
    m.sampled_f.Add(TestFunction(out) / pdf);
  }
  return m;
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

    Ray beside(PointAt(0.5f, 0.5f) + 0.5f * Normal(), kU, 0.0f, nullptr);
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

TEST_SUITE("quad/sampling") {
  TEST_CASE("quad sampling: the area is the length of cross(u, v)") {
    Quad quad(kQ, kU, kV);
    CHECK(quad.Area() ==
          doctest::Approx(glm::length(glm::cross(kU, kV))).epsilon(1e-6));
  }

  TEST_CASE("quad sampling: Pdf() is distance^2 / (cos * area)") {
    // A 2 x 2 square in the plane z = 0, whose centre is 3 away along +z.
    Quad square(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 2.0f, 0.0f));
    const glm::vec3 origin(0.0f, 0.0f, 3.0f);

    CHECK(square.Pdf(origin, glm::vec3(0.0f, 0.0f, -1.0f)) ==
          doctest::Approx(9.0f / 4.0f).epsilon(1e-6));

    // Slanted, to a point just inside the edge at x = 1.
    const glm::vec3 slanted(0.999f, 0.0f, -3.0f);
    const float distance2 = glm::dot(slanted, slanted);
    const float cosine = 3.0f / std::sqrt(distance2);
    CHECK(square.Pdf(origin, slanted) ==
          doctest::Approx(distance2 / (cosine * 4.0f)).epsilon(1e-5));
  }

  TEST_CASE(
      "quad sampling: Pdf() ignores the direction's length and the side") {
    Quad quad(kQ, kU, kV);
    const glm::vec3 p = PointAt(0.3f, 0.6f);
    const glm::vec3 above = p + 2.0f * Normal();
    const glm::vec3 below = p - 2.0f * Normal();
    const float pdf = quad.Pdf(above, p - above);
    CHECK(pdf > 0.0f);
    CHECK(quad.Pdf(above, 5.0f * (p - above)) ==
          doctest::Approx(pdf).epsilon(1e-5));
    CHECK(quad.Pdf(below, p - below) == doctest::Approx(pdf).epsilon(1e-5));
  }

  TEST_CASE("quad sampling: Pdf() is zero for a direction that misses") {
    Quad quad(kQ, kU, kV);
    const glm::vec3 origin = PointAt(0.5f, 0.5f) + Normal();
    CHECK(quad.Pdf(origin, Normal()) == 0.0f);  // pointing away
    CHECK(quad.Pdf(origin, kU) == 0.0f);        // parallel
    CHECK(quad.Pdf(origin, PointAt(1.2f, 0.5f) - origin) ==
          0.0f);  // past an edge
  }

  TEST_CASE("quad sampling: a cube face fills a sixth of the sphere") {
    // A closed-form answer, known without this code: seen from the centre, a
    // cube's six faces share the sphere evenly.
    Quad face(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(2.0f, 0.0f, 0.0f),
              glm::vec3(0.0f, 2.0f, 0.0f));
    const QuadSampling m = MeasureSampling(face, glm::vec3(0.0f), 7u);

    CHECK_ESTIMATE(m.pdf_mass, 1.0);
    CHECK_ESTIMATE(m.sampled_angle, 2.0 * kPI / 3.0);
    CHECK_ESTIMATE(m.sky_angle, 2.0 * kPI / 3.0);
    CHECK(m.missed <= kSamples / 100000);
  }

  TEST_CASE("quad sampling: SampleToward() and Pdf() agree on a skewed quad") {
    // Near and off-centre, so the density across the quad is far from flat:
    // a sampler uniform in solid angle, or Pdf() without its cosine, parts
    // company with the sky here.
    Quad quad(kQ, kU, kV);
    const glm::vec3 origin = PointAt(0.2f, 0.7f) + 0.6f * Normal();
    const QuadSampling m = MeasureSampling(quad, origin, 11u);

    CHECK_ESTIMATE(m.pdf_mass, 1.0);
    CHECK_ESTIMATES_AGREE(m.sampled_angle, m.sky_angle);
    CHECK_ESTIMATES_AGREE(m.sampled_f, m.sky_f);
    CHECK(m.missed <= kSamples / 100000);
  }

  TEST_CASE("quad sampling: a transformed quad is sampled where it landed") {
    // Non-uniform scale and a turn, so the area changes by a factor a wrong
    // edge transform would miss, and the plane lies in no axis plane.
    const glm::mat4 m =
        glm::translate(glm::vec3(4.0f, -1.0f, 2.0f)) *
        glm::rotate(glm::radians(35.0f),
                    glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f))) *
        glm::scale(glm::vec3(2.0f, 3.0f, 0.5f));
    const Quad local(kQ, kU, kV);
    const Quad world = local.Transformed(m);

    const glm::vec3 u_world(m * glm::vec4(kU, 0.0f));
    const glm::vec3 v_world(m * glm::vec4(kV, 0.0f));
    CHECK(world.Area() ==
          doctest::Approx(glm::length(glm::cross(u_world, v_world)))
              .epsilon(1e-5));

    // Each corner stays a corner: a ray at the transformed point just inside
    // it hits, and one just outside it misses.
    const glm::vec3 centre(m * glm::vec4(PointAt(0.5f, 0.5f), 1.0f));
    const glm::vec3 origin =
        centre + 5.0f * glm::normalize(glm::cross(u_world, v_world));
    const glm::vec3 in(m * glm::vec4(PointAt(0.98f, 0.02f), 1.0f));
    const glm::vec3 out(m * glm::vec4(PointAt(1.02f, -0.02f), 1.0f));
    CHECK(world.Pdf(origin, in - origin) > 0.0f);
    CHECK(world.Pdf(origin, out - origin) == 0.0f);

    const QuadSampling s = MeasureSampling(world, origin, 13u);
    CHECK_ESTIMATE(s.pdf_mass, 1.0);
    CHECK_ESTIMATES_AGREE(s.sampled_f, s.sky_f);
  }
}
