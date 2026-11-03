// A box's hit, against the six quads it replaces.
//
// One claim: on any ray, Box finds what the nearest of its six faces, built
// as quads, finds -- hit or miss, the same t, point and normal. So swapping
// a six-quad box for a Box changes no geometry, only the cost.
//
// Rays come from outside, aimed at points near the box so some graze or
// miss, and from inside, where both must find the face the ray leaves by.
// The inside case also asks as a medium does, over (-inf, inf) and then
// past the entry, since that is how a smoke boundary is queried.

#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/primitive.h"
#include "render/sampling.h"

namespace {

constexpr int kRays = 4000;
constexpr float kInf = std::numeric_limits<float>::infinity();

const glm::vec3 kLo(-1.0f, 0.5f, 2.0f);
const glm::vec3 kHi(1.5f, 3.0f, 2.75f);

// The six faces, each a quad: a corner and the two edges leaving it.
std::vector<Quad> SixQuads(const glm::vec3& lo, const glm::vec3& hi) {
  const glm::vec3 dx(hi.x - lo.x, 0.0f, 0.0f);
  const glm::vec3 dy(0.0f, hi.y - lo.y, 0.0f);
  const glm::vec3 dz(0.0f, 0.0f, hi.z - lo.z);
  std::vector<Quad> quads;
  quads.emplace_back(glm::vec3(lo.x, lo.y, hi.z), dx, dy);   // front
  quads.emplace_back(glm::vec3(hi.x, lo.y, hi.z), -dz, dy);  // right
  quads.emplace_back(glm::vec3(hi.x, lo.y, lo.z), -dx, dy);  // back
  quads.emplace_back(glm::vec3(lo.x, lo.y, lo.z), dz, dy);   // left
  quads.emplace_back(glm::vec3(lo.x, hi.y, hi.z), dx, -dz);  // top
  quads.emplace_back(glm::vec3(lo.x, lo.y, lo.z), dx, dz);   // bottom
  return quads;
}

// The nearest quad hit in (t0, t1), narrowing as a node's children do.
bool NearestQuad(std::vector<Quad>& quads, Ray& ray, float t0, float t1,
                 HitRecord& hit) {
  bool is_hit = false;
  for (Quad& quad : quads) {
    HitRecord quad_hit;
    if (quad.IsHit(ray, t0, t1, quad_hit)) {
      is_hit = true;
      t1 = quad_hit.GetT();
      hit = quad_hit;
    }
  }
  return is_hit;
}

// Asks both the same question and requires the same answer. Returns
// whether they hit.
bool SameAnswer(Box& box, std::vector<Quad>& quads, Ray& ray, float t0,
                float t1, HitRecord* box_hit_out = nullptr) {
  HitRecord box_hit, quad_hit;
  const bool box_is_hit = box.IsHit(ray, t0, t1, box_hit);
  const bool quad_is_hit = NearestQuad(quads, ray, t0, t1, quad_hit);
  REQUIRE(box_is_hit == quad_is_hit);
  if (!box_is_hit) return false;
  // Within rounding, not bit for bit: a quad reaches t through its unit
  // normal, which can land an ulp off 1.
  CHECK(box_hit.GetT() == doctest::Approx(quad_hit.GetT()).epsilon(1e-6));
  for (int axis = 0; axis < 3; ++axis) {
    CHECK(box_hit.GetNormal()[axis] ==
          doctest::Approx(quad_hit.GetNormal()[axis]).epsilon(1e-6));
    CHECK(box_hit.GetHitPoint()[axis] ==
          doctest::Approx(quad_hit.GetHitPoint()[axis]).epsilon(1e-5));
  }
  if (box_hit_out) *box_hit_out = box_hit;
  return true;
}

glm::vec3 RandomIn(Rng& rng, const glm::vec3& lo, const glm::vec3& hi) {
  return glm::vec3(rng.Range(lo.x, hi.x), rng.Range(lo.y, hi.y),
                   rng.Range(lo.z, hi.z));
}

}  // namespace

TEST_SUITE("box/hit") {
  TEST_CASE("box hit: from outside, the same as the nearest of six quads") {
    // Corners given high first: the order must not matter.
    Box box(kHi, kLo);
    std::vector<Quad> quads = SixQuads(kLo, kHi);
    const glm::vec3 centre = 0.5f * (kLo + kHi);
    const glm::vec3 half = 0.5f * (kHi - kLo);
    Rng rng(21);
    int hits = 0;
    for (int i = 0; i < kRays; ++i) {
      const glm::vec3 origin =
          centre + 3.0f * glm::length(half) * RandomUnitVector(rng);
      const glm::vec3 target =
          RandomIn(rng, centre - 1.2f * half, centre + 1.2f * half);
      Ray ray(origin, target - origin, 0.0f, &rng);
      if (SameAnswer(box, quads, ray, 1e-4f, 1e6f)) ++hits;
    }
    // Most rays aim inside the box; some must miss for the miss to count.
    CHECK(hits > kRays / 2);
    CHECK(hits < kRays);
  }

  TEST_CASE("box hit: from inside, both find the exit, as a medium asks") {
    Box box(kLo, kHi);
    std::vector<Quad> quads = SixQuads(kLo, kHi);
    Rng rng(22);
    for (int i = 0; i < kRays; ++i) {
      Ray ray(RandomIn(rng, kLo, kHi), RandomUnitVector(rng), 0.0f, &rng);
      // From inside, every ray leaves through some face.
      REQUIRE(SameAnswer(box, quads, ray, 1e-4f, 1e6f));

      // A medium's two questions: the entry over the whole line
      // (behind the origin, here), then the exit past it.
      HitRecord enter;
      REQUIRE(SameAnswer(box, quads, ray, -kInf, kInf, &enter));
      CHECK(enter.GetT() < 0.0f);
      REQUIRE(SameAnswer(box, quads, ray, enter.GetT() + 0.0001f, kInf));
    }
  }
}
