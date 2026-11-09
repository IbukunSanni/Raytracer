// Aiming at a sphere, checked against closed forms and against the sphere's
// own intersection test.
//
// Seen from outside, a sphere fills a cone of half-angle theta_max, with
// sin(theta_max) = r / d, so a direction uniform over that cone has density
// 1 / (2 pi (1 - cos theta_max)). From inside, every direction meets it. The
// claims: Pdf() is that density and zero outside the cone; SampleToward()
// draws at it; every direction it draws hits the sphere; and the density
// survives a sphere so small or far that 1 - cos theta_max is mostly rounding.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <optional>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/primitive.h"
#include "render/sampling.h"
#include "support/shape_sampling.h"
#include "support/statistics.h"

namespace {

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

// The cone's solid angle by the textbook formula, in double.
double ConeAngle(double r, double d) {
  return kTwoPi * (1.0 - std::sqrt(1.0 - (r * r) / (d * d)));
}

// A unit direction at `angle` radians from -z, leaning toward +x.
glm::vec3 FromAxis(double angle) {
  return glm::vec3(static_cast<float>(std::sin(angle)), 0.0f,
                   static_cast<float>(-std::cos(angle)));
}

}  // namespace

TEST_SUITE("sphere/sampling") {
  TEST_CASE("sphere sampling: uniform over the cone it fills, zero outside") {
    // Radius 1, three away along -z: theta_max = asin(1/3).
    const NonhierSphere ball(glm::vec3(0.0f, 0.0f, -3.0f), 1.0);
    const glm::vec3 origin(0.0f);
    const double expected = 1.0 / ConeAngle(1.0, 3.0);
    const double theta_max = std::asin(1.0 / 3.0);

    CHECK(ball.Pdf(origin, glm::vec3(0.0f, 0.0f, -1.0f)) ==
          doctest::Approx(expected).epsilon(1e-6));
    CHECK(ball.Pdf(origin, 7.0f * FromAxis(0.98 * theta_max)) ==
          doctest::Approx(expected).epsilon(1e-6));
    CHECK(ball.Pdf(origin, FromAxis(1.02 * theta_max)) == 0.0f);
    CHECK(ball.Pdf(origin, glm::vec3(0.0f, 0.0f, 1.0f)) == 0.0f);
  }

  TEST_CASE("sphere sampling: from inside, every direction at 1 / 4 pi") {
    const NonhierSphere ball(glm::vec3(0.0f, 0.0f, -3.0f), 1.0);
    const glm::vec3 inside(0.2f, -0.3f, -3.4f);
    CHECK(ball.Pdf(inside, glm::vec3(1.0f, 0.0f, 0.0f)) ==
          doctest::Approx(1.0 / (2.0 * kTwoPi)).epsilon(1e-6));

    const shape_sampling::Measurement m =
        shape_sampling::Measure(ball, inside, 19u);
    CHECK_ESTIMATE(m.pdf_mass, 1.0);
    CHECK_ESTIMATE(m.sampled_angle, 2.0 * kTwoPi);
    CHECK(m.missed == 0);
  }

  TEST_CASE("sphere sampling: SampleToward() and Pdf() agree") {
    // Close and off the axes, so the cone is wide and leans: a sampler that
    // drew cos(theta) from the wrong range, or a density missing its 2 pi,
    // parts company with the sky here.
    const glm::vec3 centre(1.0f, -0.5f, 0.25f);
    const NonhierSphere ball(centre, 1.3);
    const glm::vec3 origin = centre + glm::vec3(1.1f, 1.2f, -0.9f);
    const shape_sampling::Measurement m =
        shape_sampling::Measure(ball, origin, 17u);

    const double d = glm::length(glm::vec3(1.1f, 1.2f, -0.9f));
    CHECK_ESTIMATE(m.pdf_mass, 1.0);
    CHECK_ESTIMATE(m.sky_angle, ConeAngle(1.3, d));
    CHECK_ESTIMATES_AGREE(m.sampled_angle, m.sky_angle);
    CHECK_ESTIMATES_AGREE(m.sampled_f, m.sky_f);
    CHECK(m.missed <= shape_sampling::kSamples / 100000);
  }

  TEST_CASE("sphere sampling: every direction drawn hits the sphere") {
    // The cone and the intersection test are written separately; a cone a
    // little too wide would aim paths past the sphere's edge, where Pdf()
    // still claims to cover them.
    NonhierSphere ball(glm::vec3(1.0f, -0.5f, 0.25f), 1.3);
    const glm::vec3 origin(2.1f, 0.7f, -0.65f);
    Rng rng(23u);
    int missed = 0;
    constexpr int kDraws = 200000;
    for (int i = 0; i < kDraws; ++i) {
      Ray ray(origin, ball.SampleToward(origin, rng), 0.0f, nullptr);
      HitRecord hit;
      if (!ball.IsHit(ray, 1e-4f, 1e30f, hit)) ++missed;
    }
    CAPTURE(missed);
    CHECK(missed <= kDraws / 10000);  // a grazing ray can round off the edge
  }

  TEST_CASE("sphere sampling: a small, far sphere keeps its density") {
    // r / d = 1e-6, so cos(theta_max) is 1 - 5e-13. Taking 1 - cos as a
    // subtraction leaves about four digits; the density must keep them all.
    // The limit is the disc the sphere looks like: pi (r / d)^2 steradians.
    const NonhierSphere speck(glm::vec3(0.0f, 0.0f, -1.0e6f), 1.0);
    const double expected = 1.0 / (3.14159265358979323846 * 1e-12);
    CHECK(speck.Pdf(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)) ==
          doctest::Approx(expected).epsilon(1e-6));
  }

  TEST_CASE("sphere sampling: a similarity carries it, a stretch does not") {
    const NonhierSphere local(glm::vec3(0.5f, 0.0f, -2.0f), 0.8);
    const glm::mat4 turn = glm::rotate(
        glm::radians(40.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 0.5f)));
    const glm::mat4 similar = glm::translate(glm::vec3(3.0f, -1.0f, 2.0f)) *
                              turn * glm::scale(glm::vec3(2.5f));

    const std::optional<NonhierSphere> world = local.Transformed(similar);
    REQUIRE(world.has_value());
    const NonhierSphere expected(
        glm::vec3(similar * glm::vec4(0.5f, 0.0f, -2.0f, 1.0f)), 2.0);
    const glm::vec3 origin(-4.0f, 5.0f, 1.0f);
    Rng rng(3u);
    const glm::vec3 aims[] = {expected.SampleToward(origin, rng),
                              expected.SampleToward(origin, rng),
                              glm::vec3(0.6f, -0.7f, 0.3f)};
    for (const glm::vec3& aim : aims) {
      CAPTURE(aim);
      CHECK(world->Pdf(origin, aim) ==
            doctest::Approx(expected.Pdf(origin, aim)).epsilon(1e-5));
    }

    // A mirror keeps a sphere a sphere.
    CHECK(local.Transformed(glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f)))
              .has_value());
    // A stretch makes it an ellipsoid, which no cone describes.
    CHECK_FALSE(
        local.Transformed(turn * glm::scale(glm::vec3(1.0f, 2.0f, 1.0f)))
            .has_value());
  }
}  // TEST_SUITE sphere/sampling
