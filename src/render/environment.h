// Raytracer -- radiance arriving from outside the scene

#ifndef RAYTRACER_SRC_RENDER_ENVIRONMENT_H_
#define RAYTRACER_SRC_RENDER_ENVIRONMENT_H_

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

// A decoded lat-long (equirectangular) texture.
struct EnvironmentTexture {
  std::vector<unsigned char> rgba;  // sRGB bytes, 4 per texel
  unsigned width = 0;
  unsigned height = 0;
};

// Decodes the PNG at `path`. Returns null, and logs why, if it cannot.
std::shared_ptr<const EnvironmentTexture> LoadEnvironmentTexture(
    const std::string& path);

// What a ray that hits nothing sees.
//
// A function of direction alone, so the camera ray and every bounce ray
// see the same environment. The old lookup was screen-space, which is
// meaningless for a bounce ray -- it has no pixel -- and meant a sphere
// could never match the background the furnace test compares it to.
class Environment {
 public:
  // With a null `texture` the environment is a uniform `ambient` -- exactly
  // the furnace condition. The texture is not copied, so it must outlive
  // this object.
  Environment(const EnvironmentTexture* texture, const glm::vec3& ambient);

  // Linear radiance arriving along `dir_vec`, which need not be normalized.
  glm::vec3 Radiance(const glm::vec3& dir_vec) const;

 private:
  const EnvironmentTexture* texture_;
  glm::vec3 ambient_;
};

#endif  // RAYTRACER_SRC_RENDER_ENVIRONMENT_H_
