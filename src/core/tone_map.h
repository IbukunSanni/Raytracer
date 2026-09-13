// Tone mapping and the sRGB transfer function.
//
//   radiance -> average -> exposure -> tone curve -> sRGB encode -> bytes
//                          \______ this file ______/
//
// Two separate stages: the tone curve squeezes unbounded HDR into [0,1]
// without flattening highlights; the transfer function undoes the ~x^2.2
// decode every PNG viewer applies. Order matters -- encoding first would
// make the curve operate on warped values.
//
// Contract: linear HDR in -> linear [0,1] out. That is what makes the
// operators swappable, and why encoding stays outside (raw linear dumps
// are needed for the furnace test).

#ifndef RAYTRACER_SRC_CORE_TONE_MAP_H_
#define RAYTRACER_SRC_CORE_TONE_MAP_H_

#include <glm/glm.hpp>

namespace tonemap {

enum class Operator {
  // Clamp only.
  kNone,

  // L / (1 + L). Cheap, never clips, never quite reaches white either.
  kReinhard,

  // Reinhard with a white point: the luminance that maps to exactly 1.0.
  kReinhardExtended,

  // Not implemented; behaves as None. Would be Narkowicz's fit.
  kAces,
};

struct Config {
  Operator op = Operator::kNone;

  // Applied before the curve. Every operator expects exposed input.
  float exposure = 1.0f;

  // ReinhardExtended only: the luminance that maps to white.
  float white_point = 4.0f;

  // Off writes raw linear values.
  bool encode_srgb = true;
};

// Rec.709 luminance. Operators map this scalar and rescale the colour by
// the ratio, so R:G:B -- the hue -- is preserved.
float Luminance(const glm::vec3& linear);

// Linear HDR in, linear [0,1] out.
glm::vec3 Apply(const glm::vec3& linear, const Config& cfg);

// The transfer function, and its inverse for reading sRGB assets.
double EncodeSrgb(double linear);
double DecodeSrgb(double encoded);

}  // namespace tonemap

#endif  // RAYTRACER_SRC_CORE_TONE_MAP_H_
