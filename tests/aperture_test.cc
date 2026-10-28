// The aperture masks, checked on their own -- no camera, no scene, no image.
//
// One claim: AREA. A rejection sampler is uniform over whatever its
// predicate accepts, so the acceptance rate over the square is the mask's
// area and can be checked against the geometry in closed form. This is the
// claim a furnace test cannot make: it measures shape, not total. A mask
// drawn too wide or too narrow changes its area, so this also catches what
// a containment check would.

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

}  // TEST_SUITE render/aperture
