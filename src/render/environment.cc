// Raytracer -- radiance arriving from outside the scene

#include "render/environment.h"

#include <lodepng/lodepng.h>

#include <cmath>
#include <glm/ext.hpp>

#include "core/log.h"
#include "core/tone_map.h"
#include "render/sampling.h"

static const float kMaxRgb = 255.0f;  // 8-bit channel max

std::shared_ptr<const EnvironmentTexture> LoadEnvironmentTexture(
    const std::string& path) {
  auto texture = std::make_shared<EnvironmentTexture>();
  const unsigned error =
      lodepng::decode(texture->rgba, texture->width, texture->height, path);
  if (error) {
    // Fall back to the uniform environment rather than rendering
    // against whatever half-decoded bytes are in the buffer.
    LOG_ERROR(kRender) << "environment texture: " << lodepng_error_text(error);
    return nullptr;
  }
  return texture;
}

Environment::Environment(const EnvironmentTexture* texture,
                         const glm::vec3& ambient)
    : texture_(texture), ambient_(ambient) {
  if (texture_ == nullptr) {
    LOG_DEBUG(kRender) << "uniform environment " << glm::to_string(ambient);
  } else {
    LOG_DEBUG(kRender) << "environment texture " << texture_->width << "x"
                       << texture_->height;
  }
}

glm::vec3 Environment::Radiance(const glm::vec3& dir_vec) const {
  if (texture_ == nullptr) return ambient_;
  const int tex_w = static_cast<int>(texture_->width);
  const int tex_h = static_cast<int>(texture_->height);
  if (tex_w <= 0 || tex_h <= 0) {
    return ambient_;
  }

  // Lat-long (equirectangular): azimuth about +y to u, polar angle to v.
  const glm::vec3 d_vec = normalize(dir_vec);
  const float u = 0.5f + std::atan2(d_vec.x, -d_vec.z) / (2.0f * kPI);
  const float v = std::acos(glm::clamp(d_vec.y, -1.0f, 1.0f)) / kPI;

  const int tx = glm::clamp(static_cast<int>(u * tex_w), 0, tex_w - 1);
  const int ty = glm::clamp(static_cast<int>(v * tex_h), 0, tex_h - 1);

  const size_t idx =
      4u * (static_cast<size_t>(ty) * static_cast<size_t>(tex_w) +
            static_cast<size_t>(tx));

  // The PNG holds sRGB bytes; linearise them so they enter shading as
  // radiance. Image::SavePng re-encodes on the way out.
  const std::vector<unsigned char>& rgba = texture_->rgba;
  return glm::vec3(
      static_cast<float>(
          tonemap::DecodeSrgb(rgba[idx] / static_cast<double>(kMaxRgb))),
      static_cast<float>(
          tonemap::DecodeSrgb(rgba[idx + 1] / static_cast<double>(kMaxRgb))),
      static_cast<float>(
          tonemap::DecodeSrgb(rgba[idx + 2] / static_cast<double>(kMaxRgb))));
}
