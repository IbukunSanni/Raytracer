// Termm--Fall 2020

#include "core/image.h"

#include <lodepng/lodepng.h>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include "core/log.h"
#include "core/tone_map.h"

const unsigned int Image::kColorComponents = 3;  // Red, blue, green

//---------------------------------------------------------------------
Image::Image() : width_(0), height_(0), data_(0) {}

//---------------------------------------------------------------------
Image::Image(unsigned int width, unsigned int height)
    : width_(width), height_(height) {
  size_t num_elements = width_ * height_ * kColorComponents;
  data_ = new double[num_elements];
  memset(data_, 0, num_elements * sizeof(double));
}

//---------------------------------------------------------------------
Image::Image(const Image& other)
    : width_(other.width_),
      height_(other.height_),
      data_(other.data_ ? new double[width_ * height_ * kColorComponents] : 0) {
  if (data_) {
    std::memcpy(data_, other.data_,
                width_ * height_ * kColorComponents * sizeof(double));
  }
}

//---------------------------------------------------------------------
Image::~Image() { delete[] data_; }

//---------------------------------------------------------------------
Image& Image::operator=(const Image& other) {
  delete[] data_;

  width_ = other.width_;
  height_ = other.height_;
  data_ = (other.data_ ? new double[width_ * height_ * kColorComponents] : 0);

  if (data_) {
    std::memcpy(data_, other.data_,
                width_ * height_ * kColorComponents * sizeof(double));
  }

  return *this;
}

//---------------------------------------------------------------------
unsigned int Image::Width() const { return width_; }

//---------------------------------------------------------------------
unsigned int Image::Height() const { return height_; }

//---------------------------------------------------------------------
double Image::operator()(unsigned int x, unsigned int y, unsigned int i) const {
  return data_[kColorComponents * (width_ * y + x) + i];
}

//---------------------------------------------------------------------
double& Image::operator()(unsigned int x, unsigned int y, unsigned int i) {
  return data_[kColorComponents * (width_ * y + x) + i];
}

//---------------------------------------------------------------------
bool Image::SavePng(const std::string& filename) const {
  return SavePng(filename, tonemap::Config{});
}

//---------------------------------------------------------------------
bool Image::SavePng(const std::string& filename,
                    const tonemap::Config& cfg) const {
  // Linear radiance -> tone map -> sRGB transfer -> bytes. Both stages
  // live here, after linear averaging in Framebuffer::resolve. Clamping
  // is the tone map's job -- clamping first would flatten the curve's
  // input. See core/tone_map.h.
  std::vector<unsigned char> image(width_ * height_ * kColorComponents);

  for (unsigned int y = 0; y < height_; ++y) {
    for (unsigned int x = 0; x < width_; ++x) {
      const size_t pixel_index = kColorComponents * (width_ * y + x);

      // All three channels at once: luminance-based operators need
      // the whole colour.
      const glm::vec3 linear(static_cast<float>(data_[pixel_index + 0]),
                             static_cast<float>(data_[pixel_index + 1]),
                             static_cast<float>(data_[pixel_index + 2]));
      const glm::vec3 mapped = tonemap::Apply(linear, cfg);

      for (unsigned int c = 0; c < kColorComponents; ++c) {
        const double v = cfg.encode_srgb ? tonemap::EncodeSrgb(mapped[c])
                                         : static_cast<double>(mapped[c]);
        // +0.5 so the cast rounds instead of truncating.
        image[pixel_index + c] = static_cast<unsigned char>(255.0 * v + 0.5);
      }
    }
  }

  // lodepng won't create missing directories; it would just fail below.
  const std::filesystem::path parent =
      std::filesystem::path(filename).parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      LOG_ERROR(kImage) << "could not create output directory "
                        << parent.string() << ": " << ec.message();
      return false;
    }
  }

  // Encode the image
  unsigned error = lodepng::encode(filename, image, width_, height_, LCT_RGB);

  if (error) {
    LOG_ERROR(kImage) << "png encode failed for " << filename << ": "
                      << lodepng_error_text(error);
    return false;
  }

  return true;
}

//---------------------------------------------------------------------
const double* Image::Data() const { return data_; }

//---------------------------------------------------------------------
double* Image::Data() { return data_; }
