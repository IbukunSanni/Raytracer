#ifndef RAYTRACER_SRC_SCENE_PERLIN_H_
#define RAYTRACER_SRC_SCENE_PERLIN_H_

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// Ken Perlin's gradient noise: a random unit vector at every lattice point,
// blended across each unit cell with Hermite-smoothed weights, so it is
// smooth across cell faces and zero at every lattice point.
//
// The tables are drawn once from their own seeded generator, not the
// render's per-thread ones, so the pattern belongs to the scene: the same at
// every thread count and sample count, and from one render to the next.
class Perlin {
 public:
  explicit Perlin(uint32_t seed);

  // In [-1, 1], zero on the lattice, continuous everywhere, on a lattice
  // 1 unit on a side. Map it to [0, 1] before using it as a colour.
  float Noise(const glm::vec3& p) const;

  float Turb(const glm::vec3& p, int depth = 7) const;

 private:
  static constexpr int kPointCount = 256;

  std::array<glm::vec3, kPointCount> random_vec_;

  // Three shuffles of 0..255, one per axis. XOR-ing a cell's three entries
  // hashes its integer coordinates (i, j, k) to one index into
  // random_vec_, without storing a 3D grid.
  std::array<int, kPointCount> perm_x_;
  std::array<int, kPointCount> perm_y_;
  std::array<int, kPointCount> perm_z_;
};

#endif  // RAYTRACER_SRC_SCENE_PERLIN_H_
