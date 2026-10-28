#include "scene/perlin.h"

#include <cmath>
#include <utility>

#include "render/sampling.h"

namespace {

// 0..n-1 in random order: Fisher-Yates, from the back. Next() < 1, so
// target never reaches i + 1.
template <size_t N>
void FillPermuted(std::array<int, N>& perm, Rng& rng) {
  for (int i = 0; i < static_cast<int>(N); ++i) perm[i] = i;
  for (int i = static_cast<int>(N) - 1; i > 0; --i) {
    const int target = static_cast<int>(rng.Next() * (i + 1));
    std::swap(perm[i], perm[target]);
  }
}

// How much the corner at offset `i` (0 or 1) counts along one axis, for a
// point the fraction t of the way across: 1 - t for the near corner, t for
// the far one.
float CornerWeight(int i, float t) { return i == 1 ? t : 1.0f - t; }

// Each corner holds a gradient, a random unit vector, and contributes the
// dot product of it with the offset from that corner to the point: zero at
// the corner itself, rising along the gradient. The eight contributions are
// blended by Hermite-smoothed weights, u*u*(3 - 2u) per axis, whose slope is
// zero at 0 and 1, so neighbouring cells meet without a crease. Because every
// corner contributes zero at itself, the highs and lows fall between lattice
// points instead of on them, which hides the grid.
float PerlinInterp(const glm::vec3 corner[2][2][2], const glm::vec3& frac) {
  const glm::vec3 smooth = frac * frac * (3.0f - 2.0f * frac);
  float sum = 0.0f;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        const glm::vec3 offset = frac - glm::vec3(i, j, k);
        sum += CornerWeight(i, smooth.x) * CornerWeight(j, smooth.y) *
               CornerWeight(k, smooth.z) * glm::dot(corner[i][j][k], offset);
      }
    }
  }
  return sum;
}

}  // namespace

Perlin::Perlin(uint32_t seed) {
  Rng rng(seed);
  for (glm::vec3& gradient : random_vec_) gradient = RandomUnitVector(rng);
  FillPermuted(perm_x_, rng);
  FillPermuted(perm_y_, rng);
  FillPermuted(perm_z_, rng);
}

float Perlin::Noise(const glm::vec3& p) const {
  // The cell p is in, by its lowest corner, and how far across it p sits.
  // floor, not an int cast: truncating toward zero would merge the cells
  // either side of each axis into one double-width cell.
  const glm::vec3 cell = glm::floor(p);
  const int i = static_cast<int>(cell.x);
  const int j = static_cast<int>(cell.y);
  const int k = static_cast<int>(cell.z);
  const glm::vec3 frac = p - cell;  // each in [0, 1)

  // The cell's eight corners, each a lattice point with its own random
  // gradient. A neighbouring cell looks up the same four on their shared
  // face, so the noise has no jumps between cells. & 255 wraps the lattice
  // every 256 units, and is a correct modulo on negative ints too.
  glm::vec3 corner[2][2][2];
  for (int di = 0; di < 2; ++di) {
    for (int dj = 0; dj < 2; ++dj) {
      for (int dk = 0; dk < 2; ++dk) {
        corner[di][dj][dk] =
            random_vec_[perm_x_[(i + di) & 255] ^ perm_y_[(j + dj) & 255] ^
                        perm_z_[(k + dk) & 255]];
      }
    }
  }

  return PerlinInterp(corner, frac);
}

float Perlin::Turb(const glm::vec3& p, int depth) const {
  float sum = 0.0f;
  glm::vec3 octave_p = p;
  float weight = 1.0f;
  for (int octave = 0; octave < depth; ++octave) {
    sum += weight * Noise(octave_p);
    weight *= 0.5f;
    octave_p *= 2.0f;
  }
  // The octaves add signed values; absolute folds the zero crossings into
  // dark creases, which is what gives turbulence its look.
  return std::fabs(sum);
}
