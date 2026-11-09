// Does a shape's SampleToward() draw directions at the density its Pdf()
// reports?
//
// Three measurements of a shape as seen from one point. The sky_ ones draw
// directions uniformly over the sphere and never call SampleToward(), so the
// sampled_ ones have something independent to agree with. Any shape with
// Pdf(origin, direction) and SampleToward(origin, rng) can be measured.

#ifndef RAYTRACER_TESTS_SUPPORT_SHAPE_SAMPLING_H_
#define RAYTRACER_TESTS_SUPPORT_SHAPE_SAMPLING_H_

#include <cstdint>
#include <glm/glm.hpp>

#include "render/sampling.h"
#include "support/statistics.h"

namespace shape_sampling {

// Samples per estimate: enough that a wrong factor in the density, or a
// sampler uneven by a percent, lands well outside four standard errors.
constexpr int kSamples = 2000000;

// A function of direction that no density here is proportional to, so a
// matched pair of errors in SampleToward() and Pdf() has nothing to cancel.
inline double TestFunction(const glm::vec3& w) {
  const double lean = glm::dot(w, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
  return lean * lean;
}

struct Measurement {
  stats::Estimate pdf_mass;       // integral of Pdf() over the sphere
  stats::Estimate sampled_angle;  // mean of 1 / pdf over SampleToward()
  stats::Estimate sky_angle;      // fraction of the sphere covered, as sr
  stats::Estimate sampled_f;      // mean of f / pdf over SampleToward()
  stats::Estimate sky_f;          // integral of f over the covered directions
  int missed = 0;                 // SampleToward() draws Pdf() calls a miss
};

template <typename Shape>
Measurement Measure(const Shape& shape, const glm::vec3& origin,
                    uint32_t seed) {
  Rng rng(seed);
  Measurement m;
  const double sphere = 4.0 * kPI;
  for (int i = 0; i < kSamples; ++i) {
    const glm::vec3 w = RandomUnitVector(rng);
    const double pdf_w = shape.Pdf(origin, w);
    const bool covered = pdf_w > 0.0;
    m.pdf_mass.Add(pdf_w * sphere);
    m.sky_angle.Add(covered ? sphere : 0.0);
    m.sky_f.Add(covered ? TestFunction(w) * sphere : 0.0);

    const glm::vec3 out = shape.SampleToward(origin, rng);
    const double pdf = shape.Pdf(origin, out);
    if (pdf <= 0.0) {
      ++m.missed;
      m.sampled_angle.Add(0.0);
      m.sampled_f.Add(0.0);
      continue;
    }
    m.sampled_angle.Add(1.0 / pdf);
    m.sampled_f.Add(TestFunction(out) / pdf);
  }
  return m;
}

}  // namespace shape_sampling

#endif  // RAYTRACER_TESTS_SUPPORT_SHAPE_SAMPLING_H_
