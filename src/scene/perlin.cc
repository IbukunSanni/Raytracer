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

// Each corner weighs in by how close the point is to it, axis by axis. The
// eight weights sum to 1, so at a corner the result is that corner's value
// exactly, and two cells sharing a face agree all along it.
float TrilinearInterp(const float corner[2][2][2], float u, float v, float w) {
  float sum = 0.0f;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        sum += CornerWeight(i, u) * CornerWeight(j, v) * CornerWeight(k, w) *
               corner[i][j][k];
      }
    }
  }
  return sum;
}

}  // namespace

Perlin::Perlin(uint32_t seed) {
  Rng rng(seed);
  for (float& value : random_float_) value = rng.Next();
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
  glm::vec3 frac = p - cell;  // each in [0, 1)

  for (int i = 1; i <= 3; i++) {
    frac[i] = frac[i] * frac[i] * (3 - 2 * frac[i]);
  }

  // The cell's eight corners, each a lattice point with its own random
  // value. A neighbouring cell looks up the same four on their shared face,
  // which is why v1 has no jumps where v0 had them. & 255 wraps the lattice
  // every 256 units, and is a correct modulo on negative ints too.
  float corner[2][2][2];
  for (int di = 0; di < 2; ++di) {
    for (int dj = 0; dj < 2; ++dj) {
      for (int dk = 0; dk < 2; ++dk) {
        corner[di][dj][dk] =
            random_float_[perm_x_[(i + di) & 255] ^ perm_y_[(j + dj) & 255] ^
                          perm_z_[(k + dk) & 255]];
      }
    }
  }

  return TrilinearInterp(corner, frac.x, frac.y, frac.z);
}
