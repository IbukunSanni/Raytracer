// Textures and the surface coordinates they read, tested without a render.
//
// Four claims:
//   1. SphereUV maps the six axis points of the unit sphere to the values
//      Ray Tracing: The Next Week lists for them. They were written down
//      independently of this code, so agreeing with them is evidence.
//   2. The checker picks a cell by flooring, not truncating. Truncation sends
//      -0.5 and +0.5 to the same cell, so the two cells either side of zero
//      merge into one cell twice as wide.
//   3. An image comes back the right way up. assets/textures/uv_grid.png
//      has a different colour in each quadrant, so a flip in u or v, or a
//      missing sRGB decode, changes which colour a corner returns.
//   4. Perlin noise is part of the scene, not the render: one seed, one
//      pattern. It floors into cells the way the checker does.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <string>

#include "core/tone_map.h"
#include "geometry/sphere_uv.h"
#include "scene/perlin.h"
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
}

TEST_SUITE("texture/checker") {
  TEST_CASE("spatial checker: the cells either side of zero differ") {
    const CheckerTexture checker(1.0f, kYin, kYang);
    const glm::vec3 left = checker.Value(0, 0, glm::vec3(-0.5f, 0.5f, 0.5f));
    const glm::vec3 right = checker.Value(0, 0, glm::vec3(0.5f, 0.5f, 0.5f));
    CHECK(left != right);
  }
}

TEST_SUITE("texture/perlin") {
  TEST_CASE("perlin: one seed gives one pattern, another seed another") {
    // The tables come from their own seeded generator, so two textures
    // built alike agree at every point, on any thread, in any render.
    const Perlin a(7);
    const Perlin b(7);
    const Perlin c(8);
    int differ = 0;
    for (float x = -3.0f; x < 3.0f; x += 0.37f) {
      const glm::vec3 p(x, 0.5f * x, -x);
      CHECK(a.Noise(p) == b.Noise(p));
      if (a.Noise(p) != c.Noise(p)) ++differ;
    }
    CHECK(differ > 0);
  }

  TEST_CASE("perlin: negative cells repeat like any others") {
    // The lattice wraps every 256 units, so a point and the same point 256
    // further on read the same corners at the same fraction. Flooring puts
    // -0.3 in cell -1 at 0.7 across, as 255.7 is in cell 255 at 0.7. An int
    // cast would put it in cell 0 at -0.3 across, and the two would differ.
    // The tolerance covers the rounding of 255.7 as a float.
    const Perlin noise(7);
    for (float x = -3.0f; x < 0.0f; x += 0.23f) {
      const glm::vec3 p(x, 0.6f * x, -0.4f);
      CAPTURE(x);
      CHECK(noise.Noise(p) ==
            doctest::Approx(noise.Noise(p + glm::vec3(256.0f))).epsilon(1e-3));
    }
  }

  TEST_CASE("noise texture: grey, from the point, not (u, v)") {
    const NoiseTexture texture(1.0f, 7);  // scale 1, seed 7
    const Perlin noise(7);
    const glm::vec3 p(1.3f, -0.4f, 2.2f);
    const glm::vec3 value = texture.Value(0.1f, 0.9f, p);
    CHECK(value == glm::vec3(noise.Noise(p)));
    CHECK(texture.Value(0.7f, 0.2f, p) == value);
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
}
