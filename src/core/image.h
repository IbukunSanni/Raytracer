// Termm--Fall 2020

#ifndef RAYTRACER_SRC_CORE_IMAGE_H_
#define RAYTRACER_SRC_CORE_IMAGE_H_

#include <iostream>
#include <vector>

#include "core/tone_map.h"

// (was: typedef unsigned int uint; -- every use is now explicit,
// and a project-wide `uint` collides with the POSIX one on Linux.)

/**
 * An image, consisting of a rectangle of floating-point elements.
 * Each pixel element consists of 3 components: Red, Blue, and Green.
 *
 * This class makes it easy to save the image as a PNG file.
 * Note that colours in the range [0.0, 1.0] are mapped to the integer
 * range [0, 255] when writing PNG files.
 */
class Image {
 public:
  // Construct an empty image.
  Image();

  // Construct a black image at the given width/height.
  Image(unsigned int width, unsigned int height);

  // Copy an image.
  Image(const Image& other);

  ~Image();

  // Copy the data from one image to another.
  Image& operator=(const Image& other);

  // Returns the width of the image.
  unsigned int Width() const;

  // Returns the height of the image.
  unsigned int Height() const;

  // Retrieve a particular component from the image.
  double operator()(unsigned int x, unsigned int y, unsigned int i) const;

  // Retrieve a particular component from the image.
  double& operator()(unsigned int x, unsigned int y, unsigned int i);

  // Save this image into the PNG file with name 'filename'. Any parent
  // directories in the path that don't exist yet are created.
  // Warning: If 'filename' already exists, it will be overwritten.
  // Pixels are linear radiance; the tone map and sRGB transfer are
  // applied here. The no-argument form uses the defaults.
  // Returns false (and logs) if the directory or the encode failed.
  bool SavePng(const std::string& filename) const;
  bool SavePng(const std::string& filename, const tonemap::Config& cfg) const;

  const double* Data() const;
  double* Data();

 private:
  unsigned int width_;
  unsigned int height_;
  double* data_;

  static const unsigned int kColorComponents;
};

#endif  // RAYTRACER_SRC_CORE_IMAGE_H_
