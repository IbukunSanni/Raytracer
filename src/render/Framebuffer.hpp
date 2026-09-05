// Progressive accumulation buffer.
//
// The renderer used to loop N samples per pixel and then write the image
// once. That couples "how many samples" to "when do I get a picture": to
// see the result at 4 samples and again at 64 you had to render twice.
//
// Here the radiance sum is kept per pixel alongside a sample count, and
// dividing happens at write-out. A pass adds one sample to every pixel, so
// the buffer can be resolved to an image at any point without disturbing
// the accumulation — snapshot at 4 samples, keep going, snapshot at 64.
//
// Thread safety: a pass is split into horizontal bands and each thread owns
// a disjoint range of rows, so no two threads ever touch the same pixel.
// That is why add() needs no atomics. If the decomposition ever changes to
// overlapping tiles (staircase step 6 keeps them disjoint too), this
// assumption has to be revisited.

#pragma once

#include "core/Image.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

class Framebuffer {
public:
	Framebuffer(size_t width, size_t height);

	// Add one sample's radiance to a pixel. Does not change the sample
	// count -- a pass covers every pixel, so the count is bumped once per
	// pass by addSamples().
	void add(size_t x, size_t y, const glm::vec3 & radiance);

	// Record that every pixel has received `n` more samples.
	void addSamples(size_t n);

	size_t sampleCount() const { return m_samples; }
	size_t width()  const { return m_width; }
	size_t height() const { return m_height; }

	// Divide the accumulated sums by the sample count and write into an
	// image. Const: the buffer is unchanged, so rendering can continue.
	// Resolving before any samples have landed yields a black image
	// rather than a division by zero.
	void resolve(Image & out) const;

	// Sum is double, not float: at high sample counts a float accumulator
	// loses the low bits of each new sample once the running total grows
	// large enough, which shows up as the image quietly refusing to
	// converge.
	const std::vector<glm::dvec3> & sums() const { return m_sum; }

private:
	size_t m_width;
	size_t m_height;
	size_t m_samples;
	std::vector<glm::dvec3> m_sum;
};
