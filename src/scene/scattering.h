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

// Schlick's approximation to the Fresnel reflectance, climbing from r0 at
// normal incidence toward 1 at grazing. `cosine` must be measured on the
// thinner side of the interface, which is not always the incident side.
inline double Reflectance(double cosine, double index_ratio) {
  auto r0 = (1 - index_ratio) / (1 + index_ratio);
  r0 = r0 * r0;
  return r0 + (1 - r0) * std::pow((1 - cosine), 5);
}

#endif  // RAYTRACER_SRC_SCENE_SCATTERING_H_
