#ifndef RAYTRACER_SRC_CORE_RAY_H_
#define RAYTRACER_SRC_CORE_RAY_H_

#include <glm/glm.hpp>

class Ray {
 private:
  glm::vec3 origin_vec_;
  glm::vec3 dir_vec_;

 public:
  void SetOrigin(const glm::vec3& o_vec) { origin_vec_ = o_vec; }

  glm::vec3 GetOrigin() { return origin_vec_; }

  void SetDirection(const glm::vec3& d_vec) { dir_vec_ = d_vec; }

  glm::vec3 GetDirection() { return dir_vec_; }

  glm::vec3 GetPointAtT(float t_float) {
    return origin_vec_ + t_float * dir_vec_;
  }

  Ray() {
    origin_vec_ = glm::vec3();
    dir_vec_ = glm::vec3();
  }
};

#endif  // RAYTRACER_SRC_CORE_RAY_H_
