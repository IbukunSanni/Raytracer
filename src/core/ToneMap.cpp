#include "core/ToneMap.hpp"

#include <algorithm>
#include <cmath>

namespace tonemap {

namespace {

glm::vec3 clamp01(const glm::vec3 & c)
{
	return glm::vec3(std::min(std::max(c.r, 0.0f), 1.0f),
	                 std::min(std::max(c.g, 0.0f), 1.0f),
	                 std::min(std::max(c.b, 0.0f), 1.0f));
}

} // namespace

//---------------------------------------------------------------------------
float luminance(const glm::vec3 & linear)
{
	// Rec.709 weights -- unequal because the eye favours green.
	return 0.2126f * linear.r + 0.7152f * linear.g + 0.0722f * linear.b;
}

//---------------------------------------------------------------------------
glm::vec3 apply(const glm::vec3 & linear, const Config & cfg)
{
	// Exposure first; every curve below assumes scaled input.
	const glm::vec3 c = linear * cfg.exposure;

	glm::vec3 mapped = c;

	switch (cfg.op) {
	case Operator::None:
		// Clamp only; the clamp happens below.
		break;

	case Operator::Reinhard: {
		// L / (1 + L) on luminance, not per channel, so hue is preserved.
		const float Y = luminance(c);
		if (Y > 0.0f) {
			mapped = c * ((Y / (1.0f + Y)) / Y);
		} else {
			mapped = glm::vec3(0.0f);   // 0/0 guard
		}
		break;
	}

	case Operator::ReinhardExtended: {
		// Same curve, but luminance >= whitePoint maps to exactly 1.
		// As W -> infinity this becomes the plain form.
		const float Y = luminance(c);
		const float W = cfg.whitePoint;
		if (Y > 0.0f && W > 0.0f) {
			const float Yp = Y * (1.0f + Y / (W * W)) / (1.0f + Y);
			mapped = c * (Yp / Y);
		} else {
			mapped = glm::vec3(0.0f);
		}
		break;
	}

	case Operator::ACES:
		// TODO: Narkowicz fit. Behaves as None until then.
		break;
	}

	// Unconditional: a luminance-based operator can still push a saturated
	// channel past 1 while its luminance is in range.
	return clamp01(mapped);
}

//---------------------------------------------------------------------------
double encodeSRGB(double linear)
{
	// IEC 61966-2-1. The linear segment near black avoids the infinite
	// slope of a pure power curve at zero; it also catches negatives.
	if (linear <= 0.0031308) {
		return 12.92 * linear;
	}
	return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

//---------------------------------------------------------------------------
double decodeSRGB(double encoded)
{
	// Exact inverse of encodeSRGB, for reading sRGB assets.
	if (encoded <= 0.04045) {
		return encoded / 12.92;
	}
	return std::pow((encoded + 0.055) / 1.055, 2.4);
}

} // namespace tonemap
