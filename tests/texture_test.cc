// Textures and the surface coordinates they read, tested without a render.
//
// Two claims:
//   1. SphereUV maps the six axis points of the unit sphere to the values
//      Ray Tracing: The Next Week lists for them. They were written down
//      independently of this code, so agreeing with them is evidence.
//   2. The checkers pick a cell by flooring, not truncating. Truncation sends
//      -0.5 and +0.5 to the same cell, so the two cells either side of zero
//      merge into one cell twice as wide.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>

#include "geometry/sphere_uv.h"
#include "scene/texture.h"

namespace {

const glm::vec3 kYin(1.0f, 0.0f, 0.0f);
const glm::vec3 kYang(0.0f, 0.0f, 1.0f);

}  // namespace

TEST_SUITE("texture/sphere-uv") {
  TEST_CASE("sphere uv: the six axis points match the book") {
    struct Case {
      glm::vec3 p;
      float u, v;
    };
    const Case cases[] = {
        {{1.0f, 0.0f, 0.0f}, 0.50f, 0.50f},
        {{0.0f, 1.0f, 0.0f}, 0.50f, 1.00f},
        {{0.0f, 0.0f, 1.0f}, 0.25f, 0.50f},
        {{-1.0f, 0.0f, 0.0f}, 0.00f, 0.50f},
        {{0.0f, -1.0f, 0.0f}, 0.50f, 0.00f},
        {{0.0f, 0.0f, -1.0f}, 0.75f, 0.50f},
    };
    for (const Case& c : cases) {
      CAPTURE(c.p.x);
      CAPTURE(c.p.y);
      CAPTURE(c.p.z);
      float u = -1.0f, v = -1.0f;
      SphereUV(c.p, &u, &v);
      CHECK(u == doctest::Approx(c.u).epsilon(1e-6));
      CHECK(v == doctest::Approx(c.v).epsilon(1e-6));
    }
  }

  TEST_CASE("sphere uv: a point just off the sphere at a pole is not NaN") {
    float u = 0.0f, v = 0.0f;
    SphereUV(glm::vec3(0.0f, 1.0000001f, 0.0f), &u, &v);
    CHECK_FALSE(std::isnan(v));
    CHECK(v == doctest::Approx(1.0f));
  }
}

TEST_SUITE("texture/checker") {
  TEST_CASE("spatial checker: the cells either side of zero differ") {
    const CheckerTexture checker(1.0f, kYin, kYang);
    const glm::vec3 left = checker.Value(0, 0, glm::vec3(-0.5f, 0.5f, 0.5f));
    const glm::vec3 right = checker.Value(0, 0, glm::vec3(0.5f, 0.5f, 0.5f));
    CHECK(left != right);
  }

  TEST_CASE("spatial checker: neighbours along each axis alternate") {
    const CheckerTexture checker(2.0f, kYin, kYang);
    const glm::vec3 origin_cell = checker.Value(0, 0, glm::vec3(1.0f));
    CHECK(origin_cell == kYin);  // cell (0, 0, 0): even
    CHECK(checker.Value(0, 0, glm::vec3(3.0f, 1.0f, 1.0f)) == kYang);
    CHECK(checker.Value(0, 0, glm::vec3(1.0f, 3.0f, 1.0f)) == kYang);
    CHECK(checker.Value(0, 0, glm::vec3(1.0f, 1.0f, -1.0f)) == kYang);
  }

  TEST_CASE("uv checker: cells alternate in u and in v") {
    const UVCheckerTexture checker(4, 2, kYin, kYang);
    const glm::vec3 p(0.0f);
    CHECK(checker.Value(0.10f, 0.10f, p) == kYin);   // cell (0, 0)
    CHECK(checker.Value(0.35f, 0.10f, p) == kYang);  // cell (1, 0)
    CHECK(checker.Value(0.10f, 0.60f, p) == kYang);  // cell (0, 1)
    CHECK(checker.Value(0.35f, 0.60f, p) == kYin);   // cell (1, 1)
  }
}
