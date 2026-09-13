#ifndef RAYTRACER_SRC_SCENE_MATERIAL_H_
#define RAYTRACER_SRC_SCENE_MATERIAL_H_

#include <glm/glm.hpp>

class Rng;

// Materials are BSDFs: an eval/pdf/sample triple. Declarations only, with
// the bodies in material.cc

class Material {
 public:
  virtual ~Material();

  virtual glm::vec3 Eval(const glm::vec3& view_dir, const glm::vec3& normal,
                         const glm::vec3& out) const = 0;

  // Solid-angle density that Sample() would have drawn `out` with.
  virtual float Pdf(const glm::vec3& view_dir, const glm::vec3& normal,
                    const glm::vec3& out) const = 0;

  // Draw the next direction; writes the pdf and BRDF for the direction
  // chosen, so a caller needing both does not pay for a second dispatch.
  virtual glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir,
                           const glm::vec3& normal, float* pdf,
                           glm::vec3* brdf) const = 0;

  // A delta lobe: scatters into exactly one direction, so Pdf() and Eval()
  // carry no information and Sample() carries all of it.
  virtual bool IsSpecular() const { return false; }

 protected:
  Material();
};

// Ideal diffuse. One lobe, constant in every direction, so it looks
// equally bright from any angle: chalk, plaster, matte paint.
class LambertianMaterial : public Material {
 public:
  explicit LambertianMaterial(const glm::vec3& albedo) : albedo_(albedo) {}

  glm::vec3 Eval(const glm::vec3& view_dir, const glm::vec3& normal,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const glm::vec3& normal,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const glm::vec3& normal,
                   float* pdf, glm::vec3* brdf) const override;

 private:
  glm::vec3 albedo_;
};

// Modified Blinn-Phong: a Lambertian diffuse lobe plus a normalised
// half-vector power lobe. With kd + ks <= 1 it conserves energy. Models a
// coating over a diffuse base -- plastic, varnished wood, glossy paint.
//
// With ks = 0 this is exactly LambertianMaterial: DiffuseProbability()
// returns 1, the specular lobe is never sampled, and Eval() reduces to
// kd/pi.
class BlinnPhongMaterial : public Material {
 public:
  BlinnPhongMaterial(const glm::vec3& kd, const glm::vec3& ks, double shininess)
      : kd_(kd), ks_(ks), shininess_(static_cast<float>(shininess)) {}

  glm::vec3 Eval(const glm::vec3& view_dir, const glm::vec3& normal,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const glm::vec3& normal,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const glm::vec3& normal,
                   float* pdf, glm::vec3* brdf) const override;

 private:
  // Luminance-weighted split between sampling the two lobes. Clamped so
  // neither active lobe is starved, but a fully black lobe is skipped.
  float DiffuseProbability() const;

  static constexpr float kHalfVecEps = 1e-8f;

  glm::vec3 kd_;
  glm::vec3 ks_;
  float shininess_;
};

class MirrorMaterial : public Material {
 public:
  explicit MirrorMaterial(const glm::vec3& albedo) : albedo_(albedo) {}
  bool IsSpecular() const override;

  glm::vec3 Eval(const glm::vec3& view_dir, const glm::vec3& normal,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const glm::vec3& normal,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const glm::vec3& normal,
                   float* pdf, glm::vec3* brdf) const override;

 private:
  glm::vec3 albedo_;
};

// A mirror whose scattered direction is perturbed by a point drawn from a
// ball of radius `fuzz` centred on the reflected direction: 0 is a perfect
// mirror, 1 the widest lobe that still mostly leaves the surface, and
// anything outside that range is clamped into it. A perturbation that tips
// the direction into the surface absorbs the ray, so a high fuzz darkens
// the grazing angles, where the lobe straddles the surface.
class MetalMaterial : public Material {
 public:
  MetalMaterial(const glm::vec3& albedo, float fuzz)
      : albedo_(albedo), fuzz_(glm::clamp(fuzz, 0.0f, 1.0f)) {}
  bool IsSpecular() const override;

  glm::vec3 Eval(const glm::vec3& view_dir, const glm::vec3& normal,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const glm::vec3& normal,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const glm::vec3& normal,
                   float* pdf, glm::vec3* brdf) const override;

 private:
  glm::vec3 albedo_;
  float fuzz_;
};

class DielectricMaterial : public Material {
 public:
  explicit DielectricMaterial(float index) : index_(index) {}

  bool IsSpecular() const override;

  glm::vec3 Eval(const glm::vec3& view_dir, const glm::vec3& normal,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const glm::vec3& normal,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const glm::vec3& normal,
                   float* pdf, glm::vec3* brdf) const override;

 private:
  float index_;
};

#endif  // RAYTRACER_SRC_SCENE_MATERIAL_H_
