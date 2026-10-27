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

}  // namespace

Perlin::Perlin(uint32_t seed) {
  Rng rng(seed);
  for (float& value : random_float_) value = rng.Next();
  FillPermuted(perm_x_, rng);
  FillPermuted(perm_y_, rng);
  FillPermuted(perm_z_, rng);
}

float Perlin::Noise(const glm::vec3& p) const {
  // floor, not an int cast: truncating toward zero would merge the cells
  // either side of each axis into one double-width cell. & 255 wraps the
  // lattice every 64 units and is a correct modulo on negative ints too.
  const int i = static_cast<int>(std::floor(4.0f * p.x)) & 255;
  const int j = static_cast<int>(std::floor(4.0f * p.y)) & 255;
  const int k = static_cast<int>(std::floor(4.0f * p.z)) & 255;

  return random_float_[perm_x_[i] ^ perm_y_[j] ^ perm_z_[k]];
}
