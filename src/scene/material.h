#ifndef RAYTRACER_SRC_SCENE_MATERIAL_H_
#define RAYTRACER_SRC_SCENE_MATERIAL_H_

#include <glm/glm.hpp>
#include <memory>
#include <utility>

#include "scene/texture.h"

class Rng;
class HitRecord;

// A material is a BSDF: an Eval/Pdf/Sample triple at the surface in `hit`.

class Material {
 public:
  virtual ~Material();

  virtual glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                         const glm::vec3& out) const = 0;

  // Solid-angle density that Sample() would have drawn `out` with.
  virtual float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
                    const glm::vec3& out) const = 0;

  // Draws the next direction and writes its pdf and BRDF, so a caller
  // needing both pays for one dispatch.
  virtual glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir,
                           const HitRecord& hit, float* pdf,
                           glm::vec3* brdf) const = 0;

  // A delta lobe: all of it lives in Sample(); Eval() and Pdf() are zero.
  virtual bool IsSpecular() const { return false; }

  // The surface colour at `hit`, unshaded, for RT_VIEW=albedo. Not part of
  // the BSDF: nothing in the integrator calls it.
  virtual glm::vec3 Albedo(const HitRecord& hit) const = 0;

  // False inside a medium: there is no surface to project onto, so the
  // integrator leaves out the cosine it weights every bounce by.
  virtual bool HasSurface() const { return true; }

 protected:
  Material();
};

// Ideal diffuse: equally bright from every angle. Chalk, plaster, matte paint.
class LambertianMaterial : public Material {
 public:
  // A flat colour is a SolidColor, so every Lambertian reads a texture.
  explicit LambertianMaterial(const glm::vec3& albedo)
      : texture_(std::make_shared<SolidColor>(albedo)) {}
  explicit LambertianMaterial(std::shared_ptr<Texture> texture)
      : texture_(std::move(texture)) {}

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  // Shared: one texture can sit under many materials.
  std::shared_ptr<Texture> texture_;
};

// A Lambertian base under a normalised Blinn-Phong lobe, energy-conserving
// when kd + ks <= 1: plastic, varnished wood, glossy paint. With ks = 0 it
// is exactly LambertianMaterial.
class BlinnPhongMaterial : public Material {
 public:
  BlinnPhongMaterial(const glm::vec3& kd, const glm::vec3& ks, double shininess)
      : kd_(kd), ks_(ks), shininess_(static_cast<float>(shininess)) {}

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  // Luminance-weighted odds of sampling the diffuse lobe, clamped so neither
  // active lobe starves; a black lobe is never sampled.
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

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  glm::vec3 albedo_;
};

// A mirror whose reflection is nudged by a random vector of length `fuzz`,
// clamped to [0, 1] (0 is a perfect mirror). Nudges that tip into the
// surface absorb the ray, so high fuzz darkens grazing angles.
class MetalMaterial : public Material {
 public:
  MetalMaterial(const glm::vec3& albedo, float fuzz)
      : albedo_(albedo), fuzz_(glm::clamp(fuzz, 0.0f, 1.0f)) {}
  bool IsSpecular() const override;

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  glm::vec3 albedo_;
  float fuzz_;
};

class DielectricMaterial : public Material {
 public:
  explicit DielectricMaterial(float index) : index_(index) {}

  bool IsSpecular() const override;

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  float index_;
};

// The inside of a ConstantMedium: a ray that hits a particle leaves in a
// direction uniform over the whole sphere, whatever way it arrived. The
// texture is the albedo: the fraction scattered rather than absorbed.
class Isotropic : public Material {
 public:
  explicit Isotropic(std::shared_ptr<Texture> texture)
      : texture_(std::move(texture)) {}
  explicit Isotropic(const glm::vec3& albedo)
      : texture_(std::make_shared<SolidColor>(albedo)) {}

  bool HasSurface() const override { return false; }

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  // Shared: one texture can sit under many materials.
  std::shared_ptr<Texture> texture_;
};

// A medium that prefers a direction: the Henyey-Greenstein phase function.
// g in (-1, 1) is the mean cosine between the old and new directions: 0 is
// isotropic, g > 0 leans forward (fog, haze), g < 0 backward.
class Anisotropic : public Material {
 public:
  Anisotropic(std::shared_ptr<Texture> texture, float g)
      : texture_(std::move(texture)), g_(g) {}
  Anisotropic(const glm::vec3& albedo, float g)
      : texture_(std::make_shared<SolidColor>(albedo)), g_(g) {}

  bool HasSurface() const override { return false; }

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

  float G() const { return g_; }

 private:
  // Density over the sphere of turning by an angle with this cosine, from
  // the way the ray was travelling to the way it leaves.
  float Phase(float cos_theta) const;
  // A leaving direction drawn with density Phase(), about `forward`.
  glm::vec3 SamplePhase(Rng& rng, const glm::vec3& forward) const;

  // Shared: one texture can sit under many materials.
  std::shared_ptr<Texture> texture_;
  float g_;
};

#endif  // RAYTRACER_SRC_SCENE_MATERIAL_H_
