#ifndef RAYTRACER_SRC_SCENE_PERLIN_H_
#define RAYTRACER_SRC_SCENE_PERLIN_H_

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// Ken Perlin's lattice noise, built up as Ray Tracing: The Next Week, 5
// builds it. This is 5.1: one random value per lattice cell, no blending,
// so it reads as a grid of grey blocks -- white noise on a lattice.
//
// The tables are drawn once from their own seeded generator, not the
// render's per-thread ones, so the pattern belongs to the scene: the same at
// every thread count and sample count, and from one render to the next.
class Perlin {
 public:
  explicit Perlin(uint32_t seed);

  // In [0, 1). Constant inside each cell of a lattice 1/4 unit on a side.
  float Noise(const glm::vec3& p) const;

 private:
  static constexpr int kPointCount = 256;

  std::array<float, kPointCount> random_float_;

  // Three shuffles of 0..255, one per axis. XOR-ing a cell's three entries
  // hashes its integer coordinates (i, j, k) to one index into
  // random_float_, without storing a 3D grid.
  std::array<int, kPointCount> perm_x_;
  std::array<int, kPointCount> perm_y_;
  std::array<int, kPointCount> perm_z_;
};

#endif  // RAYTRACER_SRC_SCENE_PERLIN_H_
