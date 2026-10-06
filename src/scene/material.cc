#include "scene/material.h"

#include <algorithm>
#include <cmath>

#include "core/hit_record.h"
#include "render/sampling.h"
#include "scene/scattering.h"

Material::Material() {}

Material::~Material() {}

//----------------------------------------------------------------------
// LambertianMaterial

glm::vec3 LambertianMaterial::Eval(const glm::vec3&, const HitRecord& hit,
                                   const glm::vec3& out) const {
  const glm::vec3& normal = hit.GetNormal();
  // No contribution below the surface. Next event estimation calls Eval()
  // with light directions that Pdf() would have rejected.
  if (glm::dot(normal, out) <= 0.0f) return glm::vec3(0.0f);
  return albedo_ / kPI;
}

float LambertianMaterial::Pdf(const glm::vec3&, const HitRecord& hit,
                              const glm::vec3& out) const {
  const glm::vec3& normal = hit.GetNormal();
  const float c = glm::dot(normal, out);
  return c > 0.0f ? c / kPI : 0.0f;
}

glm::vec3 LambertianMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                     const HitRecord& hit, float* pdf_out,
                                     glm::vec3* brdf_out) const {
  const glm::vec3& normal = hit.GetNormal();
  glm::vec3 tangent, binormal;
  CreateOrthoNormalBasis(normal, &tangent, &binormal);
  const glm::vec3 dir =
      CosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);

  if (pdf_out) *pdf_out = glm::dot(normal, dir) / kPI;
  if (brdf_out) *brdf_out = Eval(view_dir, hit, dir);
  return dir;
}

//----------------------------------------------------------------------
// BlinnPhongMaterial

glm::vec3 BlinnPhongMaterial::Eval(const glm::vec3& view_dir,
                                   const HitRecord& hit,
                                   const glm::vec3& out) const {
  const glm::vec3& normal = hit.GetNormal();
  if (glm::dot(normal, view_dir) <= 0.0f || glm::dot(normal, out) <= 0.0f)
    return glm::vec3(0.0f);

  glm::vec3 spec(0.0f);
  const glm::vec3 h = view_dir + out;
  const float h_len2 = glm::dot(h, h);
  if (h_len2 > kHalfVecEps) {
    const float n_dot_h = glm::dot(normal, h * glm::inversesqrt(h_len2));
    if (n_dot_h > 0.0f)
      spec = ks_ * ((shininess_ + 2.0f) / (8.0f * kPI)) *
             std::pow(n_dot_h, shininess_);
  }
  return kd_ / kPI + spec;
}

float BlinnPhongMaterial::Pdf(const glm::vec3& view_dir, const HitRecord& hit,
                              const glm::vec3& out) const {
  const glm::vec3& normal = hit.GetNormal();
  const float n_dot_out = glm::dot(normal, out);
  if (n_dot_out <= 0.0f) return 0.0f;

  const float p_diffuse = DiffuseProbability();
  const float pdf_d = n_dot_out / kPI;

  float pdf_s = 0.0f;
  const glm::vec3 h = view_dir + out;
  const float h_len2 = glm::dot(h, h);
  if (h_len2 > kHalfVecEps) {
    const glm::vec3 h_unit = h * glm::inversesqrt(h_len2);
    const float n_dot_h = glm::dot(normal, h_unit);
    const float view_dot_h = glm::dot(view_dir, h_unit);
    if (n_dot_h > 0.0f && view_dot_h > 0.0f)
      pdf_s = (shininess_ + 1.0f) / (2.0f * kPI) *
              std::pow(n_dot_h, shininess_) / (4.0f * view_dot_h);
  }

  return p_diffuse * pdf_d + (1.0f - p_diffuse) * pdf_s;
}

glm::vec3 BlinnPhongMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                     const HitRecord& hit, float* pdf_out,
                                     glm::vec3* brdf_out) const {
  const glm::vec3& normal = hit.GetNormal();
  glm::vec3 tangent, binormal;
  CreateOrthoNormalBasis(normal, &tangent, &binormal);

  glm::vec3 out;
  if (rng.Next() < DiffuseProbability()) {
    out = CosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);
  } else {
    // Draw a half-vector from the power lobe and reflect `view_dir` about it.
    const float cos_theta_h = std::pow(rng.Next(), 1.0f / (shininess_ + 1.0f));
    const float sin_theta_h =
        std::sqrt(std::max(0.0f, 1.0f - cos_theta_h * cos_theta_h));
    const float phi = 2.0f * kPI * rng.Next();
    const glm::vec3 h = std::cos(phi) * sin_theta_h * tangent +
                        std::sin(phi) * sin_theta_h * binormal +
                        cos_theta_h * normal;
    out = Reflect(view_dir, h);
  }

  const float density = Pdf(view_dir, hit, out);
  if (pdf_out) *pdf_out = density;
  if (brdf_out)
    *brdf_out = density > 0.0f ? Eval(view_dir, hit, out) : glm::vec3(0.0f);
  return out;
}

float BlinnPhongMaterial::DiffuseProbability() const {
  const glm::vec3 luma(0.2126f, 0.7152f, 0.0722f);
  const float d = glm::dot(kd_, luma);
  const float s = glm::dot(ks_, luma);
  if (s <= 0.0f) return 1.0f;
  if (d <= 0.0f) return 0.0f;
  return std::min(0.9f, std::max(0.1f, d / (d + s)));
}

//----------------------------------------------------------------------
// MirrorMaterial

bool MirrorMaterial::IsSpecular() const { return true; }

// A delta lobe: nothing to evaluate outside Sample().
glm::vec3 MirrorMaterial::Eval(const glm::vec3&, const HitRecord&,
                               const glm::vec3&) const {
  return glm::vec3(0.0f);
}

float MirrorMaterial::Pdf(const glm::vec3&, const HitRecord&,
                          const glm::vec3&) const {
  return 0.0f;
}

glm::vec3 MirrorMaterial::Sample(Rng&, const glm::vec3& view_dir,
                                 const HitRecord& hit, float* pdf_out,
                                 glm::vec3* brdf_out) const {
  const glm::vec3& normal = hit.GetNormal();
  const glm::vec3 out = Reflect(view_dir, normal);

  // No density or cosine: brdf alone is the path weight.
  if (pdf_out) *pdf_out = 1.0f;
  if (brdf_out) *brdf_out = albedo_;
  return out;
}

//----------------------------------------------------------------------
// MetalMaterial

bool MetalMaterial::IsSpecular() const { return true; }

// A delta lobe: nothing to evaluate outside Sample().
glm::vec3 MetalMaterial::Eval(const glm::vec3&, const HitRecord&,
                              const glm::vec3&) const {
  return glm::vec3(0.0f);
}

float MetalMaterial::Pdf(const glm::vec3&, const HitRecord&,
                         const glm::vec3&) const {
  return 0.0f;
}

glm::vec3 MetalMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                const HitRecord& hit, float* pdf_out,
                                glm::vec3* brdf_out) const {
  const glm::vec3& normal = hit.GetNormal();

  // Nudging the unit mirror direction by fuzz_ sweeps a cone of half-angle
  // asin(fuzz_), widening to a hemisphere at fuzz_ 1.
  const glm::vec3 out =
      glm::normalize(Reflect(view_dir, normal) + fuzz_ * RandomUnitVector(rng));

  // A nudge into the surface absorbs the ray; zero density ends the path.
  const bool absorbed = glm::dot(normal, out) <= 0.0f;

  // No density or cosine: brdf alone is the path weight.
  if (pdf_out) *pdf_out = absorbed ? 0.0f : 1.0f;
  if (brdf_out) *brdf_out = absorbed ? glm::vec3(0.0f) : albedo_;
  return out;
}

//----------------------------------------------------------------------
// DielectricMaterial

bool DielectricMaterial::IsSpecular() const { return true; }

glm::vec3 DielectricMaterial::Eval(const glm::vec3&, const HitRecord&,
                                   const glm::vec3&) const {
  return glm::vec3(0.0f);
}

float DielectricMaterial::Pdf(const glm::vec3&, const HitRecord&,
                              const glm::vec3&) const {
  return 0.0f;
}

glm::vec3 DielectricMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                     const HitRecord& hit, float* pdf_out,
                                     glm::vec3* brdf_out) const {
  const glm::vec3& normal = hit.GetNormal();
  // Entering the dielectric from outside?
  bool front = glm::dot(view_dir, normal) > 0.0f;

  // Working normal, flipped to the side the ray arrived from.
  const glm::vec3 n = front ? normal : -normal;

  // Relative index of refraction: 1 / index_ entering, index_ leaving.
  const float index_ratio = front ? (1.0f / index_) : index_;

  // Snell has no solution once index_ratio * sin(theta) > 1, which can only
  // happen leaving the denser side: total internal reflection.
  const double cos_theta = std::fmin(glm::dot(view_dir, n), 1.0);
  const double sin_theta =
      std::sqrt(std::fmax(0.0, 1.0 - cos_theta * cos_theta));
  const bool cannot_refract = index_ratio * sin_theta > 1.0;

  const bool reflected =
      cannot_refract || (Reflectance(cos_theta, index_ratio) > rng.Next());

  const glm::vec3 out = reflected
                            ? glm::normalize(Reflect(view_dir, n))
                            : glm::normalize(Refract(view_dir, n, index_ratio));

  // Crossing an interface scales by the relative index squared; camera paths
  // carry importance, so entering divides where radiance would multiply.
  // Squared from index_, not the ratio, so more round trips cancel exactly.
  const float eta_squared = front ? 1.0f / (index_ * index_) : index_ * index_;

  if (pdf_out) *pdf_out = 1.0f;
  if (brdf_out) *brdf_out = glm::vec3(reflected ? 1.0f : eta_squared);
  return out;
}
