// The accumulation buffer, checked on its own.
//
// One claim: a NaN or infinite sample never reaches a pixel's sum. Every
// later sample adds to that sum, so one bad sample would spoil the pixel for
// the rest of the render; the buffer counts it as black and says so.

#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <limits>

#include "render/framebuffer.h"

TEST_SUITE("framebuffer") {
  TEST_CASE("framebuffer: a NaN or infinite sample is refused, not summed") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    Framebuffer buffer(2, 1);
    CHECK(buffer.Add(0, 0, glm::vec3(0.25f, 0.5f, 1.0f)));

    // One bad channel is enough to refuse the whole sample.
    CHECK_FALSE(buffer.Add(0, 0, glm::vec3(nan, 0.5f, 0.5f)));
    CHECK_FALSE(buffer.Add(0, 0, glm::vec3(0.5f, inf, 0.5f)));
    CHECK_FALSE(buffer.Add(0, 0, glm::vec3(0.5f, 0.5f, -inf)));
    CHECK(buffer.Add(0, 0, glm::vec3(0.25f, 0.5f, 1.0f)));

    // The good samples are all that landed, and the neighbour saw nothing.
    CHECK(buffer.Sums()[0] == glm::dvec3(0.5, 1.0, 2.0));
    CHECK(buffer.Sums()[1] == glm::dvec3(0.0));
  }
}  // TEST_SUITE framebuffer
