#ifndef RAYTRACER_SRC_CORE_HIT_RECORD_H_
#define RAYTRACER_SRC_CORE_HIT_RECORD_H_

#include <glm/glm.hpp>

#include "scene/material.h"

// What a ray hit: distance along the ray, the point, the surface normal
// there, and the material to shade with. Geometry writes one; the
// integrator only reads.
class HitRecord {
 public:
  HitRecord()
      : t_(0.0f), hit_point_vec_(0.0f), normal_vec_(0.0f), material_(nullptr) {}

  float GetT() const { return t_; }
  const glm::vec3& GetHitPoint() const { return hit_point_vec_; }
  const glm::vec3& GetNormal() const { return normal_vec_; }
  Material* GetMaterial() const { return material_; }

  void SetT(float t_float) { t_ = t_float; }
  void SetHitPoint(const glm::vec3& p_vec) { hit_point_vec_ = p_vec; }
  void SetNormal(const glm::vec3& n_vec) { normal_vec_ = n_vec; }
  void SetMaterial(Material* material) { material_ = material; }

  // Every primitive writes all three together.
  void SetHit(float t_float, const glm::vec3& p_vec, const glm::vec3& n_vec) {
    t_ = t_float;
    hit_point_vec_ = p_vec;
    normal_vec_ = n_vec;
  }

 private:
  float t_;
  glm::vec3 hit_point_vec_;
  glm::vec3 normal_vec_;
  Material* material_;
};

#endif  // RAYTRACER_SRC_CORE_HIT_RECORD_H_
