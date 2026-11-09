// Progressive accumulation buffer, so "how many samples" is not tied to
// "when do I get a picture".
//
// The radiance sum is kept per pixel alongside a sample count, and
// dividing happens at write-out. A pass adds one sample to every pixel, so
// the buffer can be resolved to an image at any point without disturbing
// the accumulation — snapshot at 4 samples, keep going, snapshot at 64.
//
// Thread safety: a pass is split into horizontal bands and each thread owns
// a disjoint range of rows, so no two threads ever touch the same pixel.
// That is why Add() needs no atomics. A move to tiles keeps that only while
// the tiles stay disjoint; overlapping ones would need atomics or a merge.

#ifndef RAYTRACER_SRC_RENDER_FRAMEBUFFER_H_
#define RAYTRACER_SRC_RENDER_FRAMEBUFFER_H_

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

#include "core/image.h"

class Framebuffer {
 public:
  Framebuffer(size_t width, size_t height);

  // Add one sample's radiance to a pixel. Does not change the sample
  // count -- a pass covers every pixel, so the count is bumped once per
  // pass by AddSamples().
  //
  // A NaN or infinite sample would spoil its pixel for good, since every
  // later sample adds to it, so it counts as black instead. Returns false
  // then, for the caller to count: a sample like that is a bug upstream, and
  // dropping it hides the symptom, not the fact.
  bool Add(size_t x, size_t y, const glm::vec3& radiance);

  // Record that every pixel has received `n` more samples.
  void AddSamples(size_t n);

  size_t SampleCount() const { return samples_; }
  size_t Width() const { return width_; }
  size_t Height() const { return height_; }

  // Divide the accumulated sums by the sample count and write into an
  // image. Const: the buffer is unchanged, so rendering can continue.
  // Resolving before any samples have landed yields a black image
  // rather than a division by zero.
  void Resolve(Image& out) const;

  // Sum is double, not float: at high sample counts a float accumulator
  // loses the low bits of each new sample once the running total grows
  // large enough, which shows up as the image quietly refusing to
  // converge.
  const std::vector<glm::dvec3>& Sums() const { return sum_; }

 private:
  size_t width_;
  size_t height_;
  size_t samples_;
  std::vector<glm::dvec3> sum_;
};

#endif  // RAYTRACER_SRC_RENDER_FRAMEBUFFER_H_
