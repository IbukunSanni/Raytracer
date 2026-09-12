#pragma once

#include <glm/glm.hpp>

class Rng;

// Materials are BSDFs: an eval/pdf/sample triple. Declarations only, with
// the bodies in Material.cpp

class Material
{
public:
  virtual ~Material();

  virtual glm::vec3 eval(const glm::vec3 &in,
                         const glm::vec3 &normal,
                         const glm::vec3 &out) const = 0;

  // Solid-angle density that sample() would have drawn `out` with.
  virtual float pdf(const glm::vec3 &in,
                    const glm::vec3 &normal,
                    const glm::vec3 &out) const = 0;

  // Draw the next direction; writes the pdf and BRDF for the direction
  // chosen, so a caller needing both does not pay for a second dispatch.
  virtual glm::vec3 sample(Rng &rng,
                           const glm::vec3 &in,
                           const glm::vec3 &normal,
                           float *pdf,
                           glm::vec3 *brdf) const = 0;

  // A delta lobe: scatters into exactly one direction, so pdf() and eval()
  // carry no information and sample() carries all of it.
  virtual bool isSpecular() const { return false; }

protected:
  Material();
};

// Ideal diffuse. One lobe, constant in every direction, so it looks
// equally bright from any angle: chalk, plaster, matte paint.
class LambertianMaterial : public Material
{
public:
  explicit LambertianMaterial(const glm::vec3 &albedo)
    : m_albedo(albedo)
  {
  }

  glm::vec3 eval(const glm::vec3 &in,
                 const glm::vec3 &normal,
                 const glm::vec3 &out) const override;

  float pdf(const glm::vec3 &in,
            const glm::vec3 &normal,
            const glm::vec3 &out) const override;

  glm::vec3 sample(Rng &rng,
                   const glm::vec3 &in,
                   const glm::vec3 &normal,
                   float *pdf,
                   glm::vec3 *brdf) const override;

private:
  glm::vec3 m_albedo;
};

// Modified Blinn-Phong: a Lambertian diffuse lobe plus a normalised
// half-vector power lobe. With kd + ks <= 1 it conserves energy. Models a
// coating over a diffuse base -- plastic, varnished wood, glossy paint.
//
// With ks = 0 this is exactly LambertianMaterial: diffuseProbability()
// returns 1, the specular lobe is never sampled, and eval() reduces to
// kd/pi.
class BlinnPhongMaterial : public Material
{
public:
  BlinnPhongMaterial(const glm::vec3 &kd, const glm::vec3 &ks,
                     double shininess)
    : m_kd(kd), m_ks(ks), m_shininess(static_cast<float>(shininess))
  {
  }

  glm::vec3 eval(const glm::vec3 &in,
                 const glm::vec3 &normal,
                 const glm::vec3 &out) const override;

  float pdf(const glm::vec3 &in,
            const glm::vec3 &normal,
            const glm::vec3 &out) const override;

  glm::vec3 sample(Rng &rng,
                   const glm::vec3 &in,
                   const glm::vec3 &normal,
                   float *pdf,
                   glm::vec3 *brdf) const override;

private:
  // Luminance-weighted split between sampling the two lobes. Clamped so
  // neither active lobe is starved, but a fully black lobe is skipped.
  float diffuseProbability() const;

  static constexpr float kHalfVecEps = 1e-8f;

  glm::vec3 m_kd;
  glm::vec3 m_ks;
  float m_shininess;
};
