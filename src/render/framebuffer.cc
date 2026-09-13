#include "render/framebuffer.h"

Framebuffer::Framebuffer(size_t width, size_t height)
    : width_(width),
      height_(height),
      samples_(0),
      sum_(width * height, glm::dvec3(0.0)) {}

void Framebuffer::Add(size_t x, size_t y, const glm::vec3& radiance) {
  sum_[y * width_ + x] += glm::dvec3(radiance);
}

void Framebuffer::AddSamples(size_t n) { samples_ += n; }

void Framebuffer::Resolve(Image& out) const {
  if (samples_ == 0) {
    for (size_t y = 0; y < height_; ++y) {
      for (size_t x = 0; x < width_; ++x) {
        out(static_cast<unsigned int>(x), static_cast<unsigned int>(y), 0) =
            0.0;
        out(static_cast<unsigned int>(x), static_cast<unsigned int>(y), 1) =
            0.0;
        out(static_cast<unsigned int>(x), static_cast<unsigned int>(y), 2) =
            0.0;
      }
    }
    return;
  }

  // Stays linear: Mean(f(x)) != f(Mean(x)), so averaging tone-mapped
  // samples converges on the wrong image. Curves go in Image::SavePng.
  const double inv = 1.0 / static_cast<double>(samples_);

  for (size_t y = 0; y < height_; ++y) {
    for (size_t x = 0; x < width_; ++x) {
      const glm::dvec3& s = sum_[y * width_ + x];
      out(static_cast<unsigned int>(x), static_cast<unsigned int>(y), 0) =
          s.r * inv;
      out(static_cast<unsigned int>(x), static_cast<unsigned int>(y), 1) =
          s.g * inv;
      out(static_cast<unsigned int>(x), static_cast<unsigned int>(y), 2) =
          s.b * inv;
    }
  }
}
