#ifndef RAYTRACER_SRC_CORE_RAY_H_
#define RAYTRACER_SRC_CORE_RAY_H_

#include <glm/glm.hpp>

class Rng;

// A ray is made whole, in one go: origin, direction, the instant it was
// sent and the generator it draws from.
class Ray {
 public:
  Ray() = default;
  Ray(const glm::vec3& origin, const glm::vec3& dir, float time, Rng* rng)
      : origin_vec_(origin), dir_vec_(dir), time_(time), rng_(rng) {}

  const glm::vec3& GetOrigin() const { return origin_vec_; }
  const glm::vec3& GetDirection() const { return dir_vec_; }
  // When, inside the shutter interval [0, 1), this ray was sent. A moving
  // object is hit where it is at this instant.
  float GetTime() const { return time_; }
  // The path's generator, for a medium that decides where the ray stops.
  // Null where no medium can be hit, as in a test of one primitive.
  Rng* GetRng() const { return rng_; }
  glm::vec3 GetPointAtT(float t) const { return origin_vec_ + t * dir_vec_; }

 private:
  glm::vec3 origin_vec_{0.0f};
  glm::vec3 dir_vec_{0.0f};
  float time_ = 0.0f;
  Rng* rng_ = nullptr;
};

#endif  // RAYTRACER_SRC_CORE_RAY_H_
