#include "scene/material.h"

#include <algorithm>
#include <cmath>

#include "render/sampling.h"
#include "scene/scattering.h"

Material::Material() {}

Material::~Material() {}

//----------------------------------------------------------------------
// LambertianMaterial

glm::vec3 LambertianMaterial::Eval(const glm::vec3&, const glm::vec3& normal,
                                   const glm::vec3& out) const {
  // Below the surface is no contribution. The guard matters because next
  // event estimation calls Eval() directly with a light direction that
  // Pdf() would have rejected.
  if (glm::dot(normal, out) <= 0.0f) return glm::vec3(0.0f);
  return albedo_ / kPI;
}

float LambertianMaterial::Pdf(const glm::vec3&, const glm::vec3& normal,
                              const glm::vec3& out) const {
  const float c = glm::dot(normal, out);
  return c > 0.0f ? c / kPI : 0.0f;
}

glm::vec3 LambertianMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                     const glm::vec3& normal, float* pdf_out,
                                     glm::vec3* brdf_out) const {
  glm::vec3 tangent, binormal;
  CreateOrthoNormalBasis(normal, &tangent, &binormal);
  const glm::vec3 dir =
      CosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);

  if (pdf_out) *pdf_out = glm::dot(normal, dir) / kPI;
  if (brdf_out) *brdf_out = Eval(view_dir, normal, dir);
  return dir;
}

//----------------------------------------------------------------------
// BlinnPhongMaterial

glm::vec3 BlinnPhongMaterial::Eval(const glm::vec3& view_dir,
                                   const glm::vec3& normal,
                                   const glm::vec3& out) const {
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

float BlinnPhongMaterial::Pdf(const glm::vec3& view_dir,
                              const glm::vec3& normal,
                              const glm::vec3& out) const {
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
                                     const glm::vec3& normal, float* pdf_out,
                                     glm::vec3* brdf_out) const {
  glm::vec3 tangent, binormal;
  CreateOrthoNormalBasis(normal, &tangent, &binormal);

  glm::vec3 out;
  if (rng.Next() < DiffuseProbability()) {
    out = CosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);
  } else {
    // Draw a half-vector from the Blinn-Phong power lobe, then reflect
    // `view_dir` about it to get the scattered direction.
    const float cos_theta_h = std::pow(rng.Next(), 1.0f / (shininess_ + 1.0f));
    const float sin_theta_h =
        std::sqrt(std::max(0.0f, 1.0f - cos_theta_h * cos_theta_h));
    const float phi = 2.0f * kPI * rng.Next();
    const glm::vec3 h = std::cos(phi) * sin_theta_h * tangent +
                        std::sin(phi) * sin_theta_h * binormal +
                        cos_theta_h * normal;
    out = Reflect(view_dir, h);
  }

  const float density = Pdf(view_dir, normal, out);
  if (pdf_out) *pdf_out = density;
  if (brdf_out)
    *brdf_out = density > 0.0f ? Eval(view_dir, normal, out) : glm::vec3(0.0f);
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

// A delta lobe carries no density: Eval() and Pdf() are zero everywhere,
// and Sample() is the only place any of this material's behaviour lives.
glm::vec3 MirrorMaterial::Eval(const glm::vec3&, const glm::vec3&,
                               const glm::vec3&) const {
  return glm::vec3(0.0f);
}

float MirrorMaterial::Pdf(const glm::vec3&, const glm::vec3&,
                          const glm::vec3&) const {
  return 0.0f;
}

glm::vec3 MirrorMaterial::Sample(Rng&, const glm::vec3& view_dir,
                                 const glm::vec3& normal, float* pdf_out,
                                 glm::vec3* brdf_out) const {
  const glm::vec3 out = Reflect(view_dir, normal);

  // A delta lobe: no density and no cosine, so brdf carries the whole
  // weight and the caller multiplies it straight into the throughput.
  if (pdf_out) *pdf_out = 1.0f;
  if (brdf_out) *brdf_out = albedo_;
  return out;
}

//----------------------------------------------------------------------
// MetalMaterial

bool MetalMaterial::IsSpecular() const { return true; }

// A delta lobe carries no density: Eval() and Pdf() are zero everywhere,
// and Sample() is the only place any of this material's behaviour lives.
glm::vec3 MetalMaterial::Eval(const glm::vec3&, const glm::vec3&,
                              const glm::vec3&) const {
  return glm::vec3(0.0f);
}

float MetalMaterial::Pdf(const glm::vec3&, const glm::vec3&,
                         const glm::vec3&) const {
  return 0.0f;
}

glm::vec3 MetalMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                const glm::vec3& normal, float* pdf_out,
                                glm::vec3* brdf_out) const {
  // Displacing the unit mirror direction by fuzz_ and renormalising sweeps
  // a cone of half-angle asin(fuzz_), so fuzz_ 1 is the widest lobe.
  const glm::vec3 out =
      glm::normalize(Reflect(view_dir, normal) + fuzz_ * RandomUnitVector(rng));

  // A wide perturbation can tip the direction into the surface. That ray is
  // absorbed, and zero density is how the caller is told the path ends.
  const bool absorbed = glm::dot(normal, out) <= 0.0f;

  // A delta lobe: no density and no cosine, so brdf carries the whole
  // weight and the caller multiplies it straight into the throughput.
  if (pdf_out) *pdf_out = absorbed ? 0.0f : 1.0f;
  if (brdf_out) *brdf_out = absorbed ? glm::vec3(0.0f) : albedo_;
  return out;
}

//----------------------------------------------------------------------
// DielectricMaterial

bool DielectricMaterial::IsSpecular() const { return true; }

glm::vec3 DielectricMaterial::Eval(const glm::vec3&, const glm::vec3&,
                                   const glm::vec3&) const {
  return glm::vec3(0.0f);
}

float DielectricMaterial::Pdf(const glm::vec3&, const glm::vec3&,
                              const glm::vec3&) const {
  return 0.0f;
}

glm::vec3 DielectricMaterial::Sample(Rng& rng, const glm::vec3& view_dir,
                                     const glm::vec3& normal, float* pdf_out,
                                     glm::vec3* brdf_out) const {
  // Are we entering or exiting the dielectric?
  bool front = glm::dot(view_dir, normal) > 0.0f;

  // Make the working normal always face the incoming direction.
  const glm::vec3 n = front ? normal : -normal;

  // Ratio of indices of refraction:
  // air -> material: 1 / index
  // material -> air: index / 1
  const float index_ratio = front ? (1.0f / index_) : index_;

  // Snell has no solution once index_ratio * sin(theta) exceeds 1, which
  // is reachable only when the ray is leaving the denser side. The test
  // is on index_ratio, not the index: entering, the two are reciprocals.
  const double cos_theta = std::fmin(glm::dot(view_dir, n), 1.0);
  const double sin_theta =
      std::sqrt(std::fmax(0.0, 1.0 - cos_theta * cos_theta));
  const bool cannot_refract = index_ratio * sin_theta > 1.0;

  const glm::vec3 out =
      (cannot_refract || (Reflectance(cos_theta, index_ratio) > rng.Next()))
          ? glm::normalize(Reflect(view_dir, n))
          : glm::normalize(Refract(view_dir, n, index_ratio));

  if (pdf_out) *pdf_out = 1.0f;
  if (brdf_out)
    // No Fresnel split yet -- every ray transmits, so nothing is
    // absorbed and the full radiance carries through.
    *brdf_out = glm::vec3(1.0f);
  return out;
}
