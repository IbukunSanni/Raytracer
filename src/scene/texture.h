#ifndef RAYTRACER_SRC_SCENE_TEXTURE_H_
#define RAYTRACER_SRC_SCENE_TEXTURE_H_

#include <cmath>
#include <glm/glm.hpp>
#include <memory>
#include <utility>

// A colour that varies over a surface (Ray Tracing: The Next Week, 4.1).
// Value() takes the surface coordinates and the world-space point rather
// than the HitRecord: the material unpacks the record, so a texture depends
// on nothing but where it is asked about. A procedural texture reads p; an
// image reads (u, v).
class Texture {
 public:
  virtual ~Texture() = default;

  virtual glm::vec3 Value(float u, float v, const glm::vec3& p) const = 0;
};

// The same colour everywhere: what a material's flat albedo was before
// textures, now behind the same interface as every other texture.
class SolidColor : public Texture {
 public:
  explicit SolidColor(const glm::vec3& albedo) : albedo_(albedo) {}

  glm::vec3 Value(float, float, const glm::vec3&) const override {
    return albedo_;
  }

 private:
  glm::vec3 albedo_;
};

// Cubes `scale` world units on a side, alternating between two textures by
// the hit point alone (RTNW 4.2). floor, not a cast: truncation toward zero
// would merge the two cells either side of each axis into one double cell.
class CheckerTexture : public Texture {
 public:
  explicit CheckerTexture(float scale, const glm::vec3& color_yin,
                          const glm::vec3& color_yang)
      : CheckerTexture(scale, std::make_shared<SolidColor>(color_yin),
                       std::make_shared<SolidColor>(color_yang)) {}

  CheckerTexture(float scale, std::shared_ptr<Texture> even,
                 std::shared_ptr<Texture> odd)
      : inv_scale_(1.0f / scale),
        even_(std::move(even)),
        odd_(std::move(odd)) {}

  glm::vec3 Value(float u, float v, const glm::vec3& p) const override {
    int x_int = static_cast<int>(std::floor(inv_scale_ * p.x));
    int y_int = static_cast<int>(std::floor(inv_scale_ * p.y));
    int z_int = static_cast<int>(std::floor(inv_scale_ * p.z));

    bool is_even = (x_int + y_int + z_int) % 2 == 0;

    return is_even ? even_->Value(u, v, p) : odd_->Value(u, v, p);
  }

 private:
  float inv_scale_;
  std::shared_ptr<Texture> even_;
  std::shared_ptr<Texture> odd_;
};

#endif  // RAYTRACER_SRC_SCENE_TEXTURE_H_
