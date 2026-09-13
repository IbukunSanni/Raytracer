#include "core/tone_map.h"

#include <algorithm>
#include <cmath>

namespace tonemap {

namespace {

glm::vec3 Clamp01(const glm::vec3& c) {
  return glm::vec3(std::min(std::max(c.r, 0.0f), 1.0f),
                   std::min(std::max(c.g, 0.0f), 1.0f),
                   std::min(std::max(c.b, 0.0f), 1.0f));
}

}  // namespace

//---------------------------------------------------------------------------
float Luminance(const glm::vec3& linear) {
  // Rec.709 weights -- unequal because the eye favours green.
  return 0.2126f * linear.r + 0.7152f * linear.g + 0.0722f * linear.b;
}

//---------------------------------------------------------------------------
glm::vec3 Apply(const glm::vec3& linear, const Config& cfg) {
  // Exposure first; every curve below assumes scaled input.
  const glm::vec3 c = linear * cfg.exposure;

  glm::vec3 mapped = c;

  switch (cfg.op) {
    case Operator::kNone:
      // Clamp only; the clamp happens below.
      break;

    case Operator::kReinhard: {
      // L / (1 + L) on luminance, not per channel, so hue is preserved.
      const float y = Luminance(c);
      if (y > 0.0f) {
        mapped = c * ((y / (1.0f + y)) / y);
      } else {
        mapped = glm::vec3(0.0f);  // 0/0 guard
      }
      break;
    }

    case Operator::kReinhardExtended: {
      // Same curve, but luminance >= white_point maps to exactly 1.
      // As W -> infinity this becomes the plain form.
      const float y = Luminance(c);
      const float w = cfg.white_point;
      if (y > 0.0f && w > 0.0f) {
        const float yp = y * (1.0f + y / (w * w)) / (1.0f + y);
        mapped = c * (yp / y);
      } else {
        mapped = glm::vec3(0.0f);
      }
      break;
    }

    case Operator::kAces:
      // TODO: Narkowicz fit. Behaves as None until then.
      break;
  }

  // Unconditional: a luminance-based operator can still push a saturated
  // channel past 1 while its luminance is in range.
  return Clamp01(mapped);
}

//---------------------------------------------------------------------------
double EncodeSrgb(double linear) {
  // IEC 61966-2-1. The linear segment near black avoids the infinite
  // slope of a pure power curve at zero; it also catches negatives.
  if (linear <= 0.0031308) {
    return 12.92 * linear;
  }
  return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

//---------------------------------------------------------------------------
double DecodeSrgb(double encoded) {
  // Exact inverse of EncodeSrgb, for reading sRGB assets.
  if (encoded <= 0.04045) {
    return encoded / 12.92;
  }
  return std::pow((encoded + 0.055) / 1.055, 2.4);
}

}  // namespace tonemap
