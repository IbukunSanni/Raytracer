// Tone mapping and the output transfer function -- staircase step 2.
//
// THE PIPELINE THIS BELONGS TO
//
//     radiance (unbounded, linear)          rayTraceRGB
//        -> sum / N            (linear)     Framebuffer::resolve
//        -> * exposure         (linear)      |
//        -> tone curve         (linear 0..1) |  this file
//        -> sRGB encode        (encoded)     |
//        -> * 255, to bytes                 Image::savePng
//
// Two separate stages, deliberately, because they answer different
// questions and are swapped independently:
//
//   TONE MAPPING answers "a light is 50x brighter than paper, my display
//   is not -- how do I squeeze that into 0..1 without losing the ordering
//   between bright things?" A hard clamp does not: radiance 5 and 50 both
//   become pure white, so every highlight flattens into the same blob.
//
//   THE TRANSFER FUNCTION answers "the viewer will apply roughly x^2.2 to
//   whatever bytes I write -- what do I write so it decodes back to the
//   value I meant?" Nothing to do with dynamic range; purely undoing the
//   decode every PNG viewer performs.
//
// Getting the order wrong is silent: tone map after encoding and the curve
// operates on already-warped values, so highlights compress by the wrong
// amount everywhere.
//
// THE INTERFACE CONTRACT
//
//     linear HDR in  ->  linear [0,1] out
//
// Every operator honours that, which is what makes them swappable. Adding
// ACES later is "write a function with this signature, add an enum value",
// not a pipeline change. Encoding stays outside so raw linear can still be
// dumped for the furnace test in step 3 -- a test that verifies energy
// conservation is meaningless if a curve has already reshaped the numbers.

#pragma once

#include <glm/glm.hpp>

namespace tonemap {

enum class Operator {
	// Clamp only. What the renderer does today; keeps output unchanged
	// until an operator is actually implemented.
	None,

	// L / (1 + L). Cheap, never clips, but never quite reaches white
	// either, so a scene with something meant to read as pure white looks
	// slightly grey.
	Reinhard,

	// Reinhard with a white point: the luminance that should map to
	// exactly 1.0. Same curve shape, one knob.
	ReinhardExtended,

	// Placeholder for the industry-standard curve. The real ACES is a
	// colour-space transform plus the RRT plus an output transform; what
	// is normally shipped is Narkowicz's fit -- a 3x3 matrix and a
	// rational polynomial that reproduces the look. Same signature as
	// everything above, which is the entire point of this enum.
	ACES,
};

struct Config {
	Operator op = Operator::None;

	// Applied before the curve. Orthogonal to the operator: exposure sets
	// how bright the scene is, the curve decides how the top end rolls
	// off. Every operator expects already-exposed input.
	float exposure = 1.0f;

	// ReinhardExtended only: the luminance that maps to exactly white.
	float whitePoint = 4.0f;

	// Off writes raw linear values, which is what the step 2 exit
	// criterion needs to be checkable at all.
	bool encodeSRGB = true;
};

// Rec.709 luminance weights. Tone mapping this scalar and rescaling the
// colour by the ratio keeps R:G:B proportional; running the curve on each
// channel independently compresses them by different amounts and shifts
// the hue of every saturated highlight.
float luminance(const glm::vec3 & linear);

// The swappable stage. Linear HDR in, linear [0,1] out.
glm::vec3 apply(const glm::vec3 & linear, const Config & cfg);

// The transfer function, and its inverse for reading sRGB assets (the
// background PNG in Renderer.cpp is decoded bytes that have never been
// linearised -- that is what the 0.3 fudge factor there is standing in
// for).
double encodeSRGB(double linear);
double decodeSRGB(double encoded);

} // namespace tonemap
