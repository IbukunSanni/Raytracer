#ifndef RAYTRACER_SRC_CORE_RAY_H_
#define RAYTRACER_SRC_CORE_RAY_H_

#include <glm/glm.hpp>

// A ray is made whole, in one go: origin, direction and the instant it was
// sent. There are no setters, so a ray built from another -- a bounce, a
// shadow ray, the same ray in a node's local space -- has to say what time
// it carries, and one that forgets does not compile.
class Ray {
 public:
  Ray() = default;
  Ray(const glm::vec3& origin, const glm::vec3& dir, float time)
      : origin_vec_(origin), dir_vec_(dir), time_(time) {}

  const glm::vec3& GetOrigin() const { return origin_vec_; }
  const glm::vec3& GetDirection() const { return dir_vec_; }
  // When, inside the shutter interval [0, 1), this ray was sent. A moving
  // object is hit where it is at this instant.
  float GetTime() const { return time_; }
  glm::vec3 GetPointAtT(float t) const { return origin_vec_ + t * dir_vec_; }

 private:
  glm::vec3 origin_vec_{0.0f};
  glm::vec3 dir_vec_{0.0f};
  float time_ = 0.0f;
};

#endif  // RAYTRACER_SRC_CORE_RAY_H_
