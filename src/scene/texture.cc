#include "scene/texture.h"

#include <lodepng/lodepng.h>

#include <algorithm>

#include "core/tone_map.h"

std::shared_ptr<Texture> PlaceholderTexture() {
  // sRGB 204 grey and white, the usual transparency checker, in linear.
  static const std::shared_ptr<Texture> kPlaceholder =
      std::make_shared<UVCheckerTexture>(
          16, 8,
          glm::vec3(static_cast<float>(tonemap::DecodeSrgb(204.0 / 255.0))),
          glm::vec3(1.0f));
  return kPlaceholder;
}

std::shared_ptr<ImageTexture> ImageTexture::Load(const std::string& path,
                                                 std::string* error) {
  std::vector<unsigned char> rgba;
  unsigned width = 0, height = 0;
  const unsigned code = lodepng::decode(rgba, width, height, path);
  if (code != 0) {
    *error = lodepng_error_text(code);
    return nullptr;
  }

  // Alpha is ignored: a texture here is a reflectance, not a cut-out.
  std::vector<glm::vec3> texels(static_cast<size_t>(width) * height);
  for (size_t i = 0; i < texels.size(); ++i) {
    texels[i] = glm::vec3(
        static_cast<float>(tonemap::DecodeSrgb(rgba[4 * i] / 255.0)),
        static_cast<float>(tonemap::DecodeSrgb(rgba[4 * i + 1] / 255.0)),
        static_cast<float>(tonemap::DecodeSrgb(rgba[4 * i + 2] / 255.0)));
  }

  // The constructor is private, so make_shared cannot reach it.
  return std::shared_ptr<ImageTexture>(new ImageTexture(
      static_cast<int>(width), static_cast<int>(height), std::move(texels)));
}

glm::vec3 ImageTexture::Value(float u, float v, const glm::vec3&) const {
  // Clamp rather than wrap: a (u, v) a rounding error outside [0, 1] takes
  // the edge texel instead of the opposite edge's.
  u = std::clamp(u, 0.0f, 1.0f);
  v = 1.0f - std::clamp(v, 0.0f, 1.0f);

  // u = 1 would index one past the last column; it belongs to the last one.
  const int i = std::min(static_cast<int>(u * width_), width_ - 1);
  const int j = std::min(static_cast<int>(v * height_), height_ - 1);
  return texels_[static_cast<size_t>(j) * width_ + i];
}
