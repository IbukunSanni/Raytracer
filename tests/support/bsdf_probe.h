// Ways to interrogate a material, so each test body reads as a claim
// rather than as a loop.
//
// Convention: `view_dir` and `out` are unit vectors that both point AWAY
// from the surface. The normal is +z throughout, so an incident angle is a
// rotation in the xz plane and nothing needs a basis.

#ifndef RAYTRACER_TESTS_SUPPORT_BSDF_PROBE_H_
#define RAYTRACER_TESTS_SUPPORT_BSDF_PROBE_H_

#include "render/sampling.h"
#include "scene/material.h"
#include "support/statistics.h"

namespace probe {

// Samples per estimate. Enough that the standard error is a few parts in
// ten thousand, which is what makes the derived tolerances tight enough to
// catch a wrong normalisation factor.
constexpr int kSamples = 2000000;

// The surface normal every test is written against.
const glm::vec3 kNormal(0.0f, 0.0f, 1.0f);

// An incident direction at `degrees` from the normal.
inline glm::vec3 Incident(float degrees) {
  const float theta = glm::radians(degrees);
  return glm::vec3(std::sin(theta), 0.0f, std::cos(theta));
}

// Uniform on the hemisphere around the normal. p(w) = 1/(2*pi).
inline glm::vec3 UniformHemisphere(Rng& rng) {
  const float z = rng.Next();
  const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
  const float phi = 2.0f * kPI * rng.Next();
  return glm::vec3(r * std::cos(phi), r * std::sin(phi), z);
}

//---------------------------------------------------------------------
// Directional albedo: rho = integral of Eval() * cos(theta) dw, the
// fraction of arriving light the surface sends back.
//
// Sampled UNIFORMLY, so the material's own sampler and pdf stay out of the
// loop and this measures Eval() against nothing but the geometry.
inline stats::Estimate3 DirectionalAlbedo(const Material& mat,
                                          const glm::vec3& view_dir,
                                          uint32_t seed) {
  Rng rng(seed);
  stats::Estimate3 rho;
  for (int i = 0; i < kSamples; ++i) {
    const glm::vec3 w = UniformHemisphere(rng);
    rho.Add(mat.Eval(view_dir, kNormal, w) * glm::dot(kNormal, w) *
            (2.0f * kPI));
  }
  return rho;
}

//---------------------------------------------------------------------
// Measurements that catch Sample() and Pdf() disagreeing, gathered in one
// pass so a test can assert on whichever part it means.
//
// The obvious estimator, the mean of eval*cos/pdf, is useless for this: on
// a cosine-sampled diffuse the pi and the cosine cancel algebraically, so
// it returns the albedo on every draw even when Sample() returns garbage.
// Nothing below has that cancellation.
struct SamplerAgreement {
  // Integral of Pdf() over the upper hemisphere, and the fraction of
  // Sample() draws that land there. Both measure the same thing, the
  // mass above the horizon, from opposite ends. A material that never
  // scatters downwards puts both at 1; one that sometimes does puts both
  // below it, and they still have to match.
  stats::Estimate pdf_mass;
  stats::Estimate fraction_above;

  // A test function the pdf is not proportional to, so nothing can
  // cancel. Integral of cos^2(theta) dw over the hemisphere is 2*pi/3,
  // and a wrong Jacobian moves this without moving the pair above.
  stats::Estimate cos_squared_integral;
};

inline SamplerAgreement MeasureSampler(const Material& mat,
                                       const glm::vec3& view_dir,
                                       uint32_t seed) {
  Rng rng(seed);
  SamplerAgreement m;

  for (int i = 0; i < kSamples; ++i) {
    const glm::vec3 w = UniformHemisphere(rng);
    m.pdf_mass.Add(static_cast<double>(mat.Pdf(view_dir, kNormal, w)) *
                   (2.0 * kPI));

    float pdf = 0.0f;
    glm::vec3 brdf(0.0f);
    const glm::vec3 out = mat.Sample(rng, view_dir, kNormal, &pdf, &brdf);
    const float cos_out = glm::dot(kNormal, out);

    m.fraction_above.Add(cos_out > 0.0f ? 1.0 : 0.0);
    m.cos_squared_integral.Add(pdf > 0.0f
                                   ? static_cast<double>(cos_out * cos_out) /
                                         static_cast<double>(pdf)
                                   : 0.0);
  }
  return m;
}

//---------------------------------------------------------------------
// One draw, for materials with nothing to average: a delta lobe returns
// the same weight every time, so a single sample either satisfies the
// contract exactly or does not.
struct Draw {
  glm::vec3 direction;
  float pdf = 0.0f;
  glm::vec3 brdf{0.0f};
};

inline Draw DrawOnce(const Material& mat, const glm::vec3& view_dir,
                     uint32_t seed) {
  Rng rng(seed);
  Draw d;
  d.direction = mat.Sample(rng, view_dir, kNormal, &d.pdf, &d.brdf);
  return d;
}

}  // namespace probe

#endif  // RAYTRACER_TESTS_SUPPORT_BSDF_PROBE_H_
