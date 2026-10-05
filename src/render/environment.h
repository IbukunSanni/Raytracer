// Raytracer -- radiance arriving from outside the scene

#ifndef RAYTRACER_SRC_RENDER_ENVIRONMENT_H_
#define RAYTRACER_SRC_RENDER_ENVIRONMENT_H_

#include <glm/glm.hpp>
#include <string>
#include <vector>

// What a ray that hits nothing sees.
//
// A function of direction alone, so the camera ray and every bounce ray
// see the same environment. The old lookup was screen-space, which is
// meaningless for a bounce ray -- it has no pixel -- and meant a sphere
// could never match the background the furnace test compares it to.
class Environment {
 public:
  // Loads the lat-long texture at `path`. With an empty path, or one that
  // fails to decode, the environment is a uniform `ambient` -- exactly the
  // furnace condition.
  Environment(const std::string& path, const glm::vec3& ambient);

  // Linear radiance arriving along `dir_vec`, which need not be normalized.
  glm::vec3 Radiance(const glm::vec3& dir_vec) const;

 private:
  glm::vec3 ambient_;
  std::vector<unsigned char> rgba_;  // sRGB bytes, 4 per texel
  unsigned width_ = 0;               // 0 => uniform `ambient_`
  unsigned height_ = 0;
};

#endif  // RAYTRACER_SRC_RENDER_ENVIRONMENT_H_
