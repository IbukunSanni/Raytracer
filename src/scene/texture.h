#ifndef RAYTRACER_SRC_SCENE_TEXTURE_H_
#define RAYTRACER_SRC_SCENE_TEXTURE_H_

#include <cmath>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

  CheckerTexture(float scale, std::shared_ptr<Texture> yin,
                 std::shared_ptr<Texture> yang)
      : inv_scale_(1.0f / scale),
        yin_(std::move(yin)),
        yang_(std::move(yang)) {}

  glm::vec3 Value(float u, float v, const glm::vec3& p) const override {
    int x_int = static_cast<int>(std::floor(inv_scale_ * p.x));
    int y_int = static_cast<int>(std::floor(inv_scale_ * p.y));
    int z_int = static_cast<int>(std::floor(inv_scale_ * p.z));

    // Yin fills the cubes whose index sum is even, yang the odd ones.
    bool is_even = (x_int + y_int + z_int) % 2 == 0;

    return is_even ? yin_->Value(u, v, p) : yang_->Value(u, v, p);
  }

 private:
  float inv_scale_;
  std::shared_ptr<Texture> yin_;
  std::shared_ptr<Texture> yang_;
};

// A checkerboard in the surface's (u, v), `columns` cells around and `rows`
// from bottom to top. Unlike CheckerTexture it is painted on the surface, not
// cut from space: on a sphere the cells follow the lines of longitude and
// latitude and pinch together at the poles, and a seam shows wherever u
// wraps. That is what it is for -- it makes a surface's UV layout visible.
class UVCheckerTexture : public Texture {
 public:
  UVCheckerTexture(int columns, int rows, const glm::vec3& color_yin,
                   const glm::vec3& color_yang)
      : UVCheckerTexture(columns, rows, std::make_shared<SolidColor>(color_yin),
                         std::make_shared<SolidColor>(color_yang)) {}

  UVCheckerTexture(int columns, int rows, std::shared_ptr<Texture> yin,
                   std::shared_ptr<Texture> yang)
      : columns_(columns),
        rows_(rows),
        yin_(std::move(yin)),
        yang_(std::move(yang)) {}

  glm::vec3 Value(float u, float v, const glm::vec3& p) const override {
    // A u or v of exactly 1 counts as one cell past the edge. That is a line
    // of zero width, so it never shows.
    const int column = static_cast<int>(std::floor(u * columns_));
    const int row = static_cast<int>(std::floor(v * rows_));

    const bool is_even = (column + row) % 2 == 0;
    return is_even ? yin_->Value(u, v, p) : yang_->Value(u, v, p);
  }

 private:
  int columns_;
  int rows_;
  std::shared_ptr<Texture> yin_;
  std::shared_ptr<Texture> yang_;
};

// What an image texture becomes when its file is missing or will not load:
// the grey-and-white checkerboard an image editor draws behind transparent
// pixels, painted in (u, v) so it wraps a surface the way the image would
// have. Shared, so every missing image in a scene is the same texture.
std::shared_ptr<Texture> PlaceholderTexture();

// An image wrapped onto the surface's (u, v), looked up nearest-neighbour
// (Ray Tracing: The Next Week, 4.4-4.5). u runs left to right across the
// image and v bottom to top, so v is flipped against the image's top-down
// rows.
//
// The file holds sRGB bytes. Load() decodes every texel to linear once, so
// a lookup is an index and nothing more, and shading sees reflectance in
// the same linear space as every other colour.
class ImageTexture : public Texture {
 public:
  // A PNG, through lodepng. Returns null and fills `error` when the file is
  // missing or not a PNG; the caller decides what stands in for it.
  static std::shared_ptr<ImageTexture> Load(const std::string& path,
                                            std::string* error);

  glm::vec3 Value(float u, float v, const glm::vec3& p) const override;

  int Width() const { return width_; }
  int Height() const { return height_; }

 private:
  ImageTexture(int width, int height, std::vector<glm::vec3> texels)
      : width_(width), height_(height), texels_(std::move(texels)) {}

  int width_;
  int height_;
  std::vector<glm::vec3> texels_;  // linear, row 0 at the top of the image
};

#endif  // RAYTRACER_SRC_SCENE_TEXTURE_H_
