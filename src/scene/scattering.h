#ifndef RAYTRACER_SRC_SCENE_SCATTERING_H_
#define RAYTRACER_SRC_SCENE_SCATTERING_H_

#include <cmath>
#include <glm/glm.hpp>

// Scattering primitives shared between materials.
//
// The physics lives here rather than inside any one material, so that
// adding a material is lobe-selection logic and nothing else. Fresnel and
// refraction live here alongside reflection, so a dielectric assembles one
// from the other rather than deriving either itself.

// Reflect `view_dir` about `normal`. Both point AWAY from the surface, and so
// does the result. The normal is the surface normal for a mirror and the
// half-vector for a glossy lobe -- one formula, two uses.
inline glm::vec3 Reflect(const glm::vec3& view_dir, const glm::vec3& normal) {
  return 2.0f * glm::dot(view_dir, normal) * normal - view_dir;
}

inline glm::vec3 Refract(const glm::vec3& view_dir, const glm::vec3& normal,
                         float index_ratio) {
  // 1.0f, not 1.0: fmin(float, double) returns a double, and narrowing it
  // back is warning C4244 under MSVC.
  float cos_theta = std::fmin(glm::dot(view_dir, normal), 1.0f);

  glm::vec3 perpendicular = -index_ratio * (view_dir - cos_theta * normal);

  glm::vec3 parallel =
      static_cast<float>(-std::sqrt(
          std::fmax(0.0, 1.0 - glm::dot(perpendicular, perpendicular)))) *
      normal;

  return perpendicular + parallel;
}

// The fraction of energy reflected at a dielectric interface, averaged over
// the two polarisations. `cosine` is the incident one; the formula is written
// in the same index_ratio Refract() takes, so it needs no other argument.
//
// Exact rather than Schlick's approximation, which is wrong at both ends of
// this material's range: it reflects at grazing even when index_ratio is 1
// and there is no interface, and it stays near r0 as a ray approaches the
// critical angle from inside, where the true value is already climbing to 1.
inline double Reflectance(double cosine, double index_ratio) {
  const double sin_t =
      index_ratio * std::sqrt(std::fmax(0.0, 1.0 - cosine * cosine));

  // No transmitted ray exists past the critical angle, so all of it reflects.
  if (sin_t >= 1.0) return 1.0;

  const double cos_t = std::sqrt(1.0 - sin_t * sin_t);
  const double perpendicular =
      (index_ratio * cosine - cos_t) / (index_ratio * cosine + cos_t);
  const double parallel =
      (cosine - index_ratio * cos_t) / (cosine + index_ratio * cos_t);
  return 0.5 * (perpendicular * perpendicular + parallel * parallel);
}

#endif  // RAYTRACER_SRC_SCENE_SCATTERING_H_
