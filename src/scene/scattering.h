#ifndef RAYTRACER_SRC_SCENE_SCATTERING_H_
#define RAYTRACER_SRC_SCENE_SCATTERING_H_

#include <cmath>
#include <glm/glm.hpp>

// Scattering primitives shared between materials.
//
// The physics lives here rather than inside any one material, so that
// adding a material is lobe-selection logic and nothing else. Fresnel and
// refraction join this file with the dielectric.

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

#endif  // RAYTRACER_SRC_SCENE_SCATTERING_H_
