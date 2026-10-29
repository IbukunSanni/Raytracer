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

  // Radiance the surface gives off at `hit`, apart from anything it
  // reflects. Zero for every material that only scatters.
  virtual glm::vec3 Emitted(const HitRecord&) const { return glm::vec3(0.0f); }

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

// Gives off light and reflects none: a lamp, a light panel. The texture is
// radiance, not reflectance, so values above 1 are the point.
class DiffuseLight : public Material {
 public:
  explicit DiffuseLight(std::shared_ptr<Texture> texture)
      : texture_(std::move(texture)) {}
  explicit DiffuseLight(const glm::vec3& radiance)
      : texture_(std::make_shared<SolidColor>(radiance)) {}

  glm::vec3 Eval(const glm::vec3& view_dir, const HitRecord& hit,
                 const glm::vec3& out) const override;

  float Pdf(const glm::vec3& view_dir, const HitRecord& hit,
            const glm::vec3& out) const override;

  glm::vec3 Sample(Rng& rng, const glm::vec3& view_dir, const HitRecord& hit,
                   float* pdf, glm::vec3* brdf) const override;

  glm::vec3 Emitted(const HitRecord& hit) const override;

  glm::vec3 Albedo(const HitRecord& hit) const override;

 private:
  // Shared: one texture can sit under many materials.
  std::shared_ptr<Texture> texture_;
};

// Scatters inside a medium, equally in every direction. The texture is the
// fraction of light scattered rather than absorbed.
class VolumeScatter : public Material {
 public:
  explicit VolumeScatter(std::shared_ptr<Texture> texture)
      : texture_(std::move(texture)) {}
  explicit VolumeScatter(const glm::vec3& albedo)
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

#endif  // RAYTRACER_SRC_SCENE_MATERIAL_H_
