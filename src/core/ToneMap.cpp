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
	// Rec.709 weights. Not equal because the eye is far more sensitive to
	// green; a plain average would make a saturated blue and a saturated
	// green of the same "brightness" tone map by wildly different amounts.
	return 0.2126f * linear.r + 0.7152f * linear.g + 0.0722f * linear.b;
}

//---------------------------------------------------------------------------
glm::vec3 apply(const glm::vec3 & linear, const Config & cfg)
{
	// Exposure first, always. Every curve below assumes its input has
	// already been scaled.
	const glm::vec3 c = linear * cfg.exposure;

	glm::vec3 mapped = c;

	switch (cfg.op) {
	case Operator::None:
		// Clamp only. Kept as an operator so "no tone mapping" is a
		// choice rather than the absence of a stage, and so an A/B is one
		// enum apart. The clamp itself happens below.
		break;

	case Operator::Reinhard: {
		// L / (1 + L), on luminance rather than per channel so the R:G:B
		// ratio -- the hue -- is preserved. Per-channel Reinhard turns
		// (5,3,1) into (0.83,0.75,0.5): each channel compressed by a
		// different factor, so the highlight shifts hue as it dims.
		const float Y = luminance(c);
		if (Y > 0.0f) {
			mapped = c * ((Y / (1.0f + Y)) / Y);
		} else {
			mapped = glm::vec3(0.0f);   // 0/0 guard: black stays black
		}
		break;
	}

	case Operator::ReinhardExtended: {
		// Same shape with a white point: luminance at or above whitePoint
		// maps to exactly 1 instead of only approaching it, so a scene
		// that should contain pure white is not left looking washed out.
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
		// TODO(later): Narkowicz fit, or the full RRT+ODT if this ever
		// wants to be colour-managed. Behaves as None until then.
		break;
	}

	// The contract is linear [0,1] out. A luminance-based operator can
	// still push a saturated channel past 1 while its luminance is in
	// range, so the clamp is unconditional -- a caller that trusted the
	// contract would otherwise write garbage bytes.
	return clamp01(mapped);
}

//---------------------------------------------------------------------------
double encodeSRGB(double linear)
{
	// The IEC 61966-2-1 curve. The short linear segment near black exists
	// because a pure power curve has infinite slope at zero, which
	// quantises badly in 8 bits. Negatives fall through the linear branch,
	// so pow() never sees one.
	if (linear <= 0.0031308) {
		return 12.92 * linear;
	}
	return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

//---------------------------------------------------------------------------
double decodeSRGB(double encoded)
{
	// The exact inverse of encodeSRGB, for reading sRGB image assets into
	// the linear pipeline (the background sampler in Renderer.cpp).
	if (encoded <= 0.04045) {
		return encoded / 12.92;
	}
	return std::pow((encoded + 0.055) / 1.055, 2.4);
}

} // namespace tonemap
