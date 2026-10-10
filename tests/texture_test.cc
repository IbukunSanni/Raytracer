// Textures and the surface coordinates they read, tested without a render.
//
// Three claims:
//   1. SphereUV maps the six axis points of the unit sphere to the values
//      Ray Tracing: The Next Week lists for them. They were written down
//      independently of this code, so agreeing with them is evidence.
//   2. The checkers pick a cell by flooring, not truncating. Truncation sends
//      -0.5 and +0.5 to the same cell, so the two cells either side of zero
//      merge into one cell twice as wide.
//   3. An image comes back the right way up. assets/textures/uv_grid.png
//      has a different colour in each quadrant, so a flip in u or v, or a
//      missing sRGB decode, changes which colour a corner returns.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <string>

#include "core/tone_map.h"
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

TEST_SUITE("texture/image") {
  // Light cells are full-intensity bytes, which decode to exactly 1. Dark
  // cells are byte 127, which decodes to well under half: sRGB is not linear.
  const float kDark = static_cast<float>(tonemap::DecodeSrgb(127.0 / 255.0));

  TEST_CASE("image texture: each corner of the image is its (u, v) corner") {
    std::string error;
    const auto image =
        ImageTexture::Load("assets/textures/uv_grid.png", &error);
    const bool loaded = image != nullptr;  // doctest cannot print a pointer
    REQUIRE_MESSAGE(loaded, error);
    REQUIRE(image->Width() == 512);
    REQUIRE(image->Height() == 256);

    const glm::vec3 p(0.0f);
    // v = 1 is the top row of the image, u = 0 its left column.
    CHECK(image->Value(0.01f, 0.99f, p) == glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK(image->Value(0.99f, 0.99f, p) == glm::vec3(0.0f, kDark, 0.0f));
    CHECK(image->Value(0.01f, 0.01f, p) == glm::vec3(0.0f, 0.0f, kDark));
    CHECK(image->Value(0.99f, 0.01f, p) == glm::vec3(1.0f, 1.0f, 0.0f));
  }

  TEST_CASE("image texture: (u, v) at and past the edges takes the edge") {
    std::string error;
    const auto image =
        ImageTexture::Load("assets/textures/uv_grid.png", &error);
    const bool loaded = image != nullptr;  // doctest cannot print a pointer
    REQUIRE_MESSAGE(loaded, error);

    const glm::vec3 p(0.0f);
    CHECK(image->Value(1.0f, 0.0f, p) == image->Value(0.99f, 0.01f, p));
    CHECK(image->Value(-0.5f, 1.5f, p) == image->Value(0.01f, 0.99f, p));
  }

  TEST_CASE("image texture: the placeholder is a grey and white uv checker") {
    const auto placeholder = PlaceholderTexture();
    const float grey = static_cast<float>(tonemap::DecodeSrgb(204.0 / 255.0));
    const glm::vec3 p(0.0f);
    // 16 columns by 8 rows: cell (0, 0) is grey, its neighbours white.
    CHECK(placeholder->Value(0.01f, 0.01f, p) == glm::vec3(grey));
    CHECK(placeholder->Value(0.07f, 0.01f, p) == glm::vec3(1.0f));
    CHECK(placeholder->Value(0.01f, 0.13f, p) == glm::vec3(1.0f));
  }

  TEST_CASE("image texture: a missing file is an error, not an image") {
    std::string error;
    const auto image =
        ImageTexture::Load("assets/textures/missing.png", &error);
    const bool loaded = image != nullptr;
    CHECK_FALSE(loaded);
    CHECK_FALSE(error.empty());
  }
}
