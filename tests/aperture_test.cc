// The aperture masks, checked on their own -- no camera, no scene, no image.
//
// Three claims, and the first two are the ones the disk sampler's story
// turned on.
//
//   1. CONTAINMENT. Every mask is inscribed in the unit circle, because
//      the camera multiplies what it draws by the lens radius. A draw
//      outside that circle is an opening wider than the scene asked for,
//      and the drawn point must satisfy the predicate it came from.
//
//   2. AREA. A rejection sampler is uniform over whatever its predicate
//      accepts, so the acceptance rate over the square is the mask's area
//      and can be checked against the geometry in closed form. This is the
//      claim a furnace test cannot make: it measures shape, not total.
//
//   3. THE DISK IS UNCHANGED. The default aperture still forwards to
//      SampleUnitDisk, draw for draw, which is what keeps a scene that
//      asks for no shape rendering exactly as it did before shapes existed.

#include <algorithm>
#include <glm/glm.hpp>
#include <string>

#include "render/aperture.h"
#include "support/statistics.h"

namespace {

struct Mask {
  ApertureShape shape;
  std::string name;  // a std::string, so a failure prints it, not its address
  double area;
};

// Areas in closed form: pi for the disk, the shoelace formula on each
// vertex table, and the sextic's polar integral divided by the scale the
// header applies to it.
const Mask kMasks[] = {
    {ApertureShape::kDisk, "disk", 3.14159265},
    {ApertureShape::kHexagon, "hexagon", 2.59807621},
    {ApertureShape::kStar, "star", 1.12256994},
    {ApertureShape::kHeart, "heart", 1.80438507},
    {ApertureShape::kCrown, "crown", 1.06610364},
};

// Enough that the area tolerance closes to about 0.4% of the narrowest
// mask, which is far tighter than any shape error could hide in.
constexpr int kDraws = 400000;

}  // namespace

//=====================================================================
TEST_SUITE("render/aperture") {
  TEST_CASE("aperture: every mask is inscribed in the unit circle") {
    for (const Mask& mask : kMasks) {
      INFO("mask: ", mask.name);
      Rng rng(7u);
      float worst = 0.0f;
      bool all_inside = true;

      for (int i = 0; i < kDraws; ++i) {
        const glm::vec2 p = SampleAperture(mask.shape, rng);
        worst = std::max(worst, glm::length(p));
        if (!InsideAperture(mask.shape, p)) all_inside = false;
      }

      INFO("farthest draw was at radius ", worst);
      CHECK(worst <= 1.0f);
      CHECK(all_inside);
    }
  }

  TEST_CASE("aperture: the acceptance rate is the mask's area") {
    for (const Mask& mask : kMasks) {
      INFO("mask: ", mask.name);
      Rng rng(11u);
      stats::Estimate area;

      // The square is 4 units of area, so the indicator scaled by 4
      // averages to the area the predicate accepts.
      for (int i = 0; i < kDraws; ++i) {
        const glm::vec2 p(rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f));
        area.Add(InsideAperture(mask.shape, p) ? 4.0 : 0.0);
      }

      CHECK_ESTIMATE(area, mask.area);
    }
  }

  TEST_CASE("aperture: the disk case draws what the disk sampler draws") {
    Rng shaped(23u);
    Rng plain(23u);
    bool identical = true;

    for (int i = 0; i < kDraws; ++i) {
      const glm::vec2 a = SampleAperture(ApertureShape::kDisk, shaped);
      const glm::vec2 b = SampleUnitDisk(plain);
      if (a.x != b.x || a.y != b.y) identical = false;
    }

    CHECK(identical);
  }

  TEST_CASE("aperture: every shape name parses and an unknown one is refused") {
    ApertureShape shape = ApertureShape::kDisk;

    for (const Mask& mask : kMasks) {
      INFO("mask: ", mask.name);
      shape = ApertureShape::kCrown;
      CHECK(ApertureShapeFromName(mask.name, &shape));
      CHECK(shape == mask.shape);
    }

    CHECK_FALSE(ApertureShapeFromName("triangle", &shape));
    CHECK_FALSE(ApertureShapeFromName("", &shape));
    CHECK_FALSE(ApertureShapeFromName("Disk", &shape));
  }
}  // TEST_SUITE render/aperture
