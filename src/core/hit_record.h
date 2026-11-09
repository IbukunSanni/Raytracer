#ifndef RAYTRACER_SRC_CORE_HIT_RECORD_H_
#define RAYTRACER_SRC_CORE_HIT_RECORD_H_

#include <glm/glm.hpp>

#include "core/ray.h"
#include "scene/material.h"
#include "scene/texture.h"

// What a ray hit: distance along the ray, the point, the surface normal
// there, the surface coordinates (u, v), which side of the surface the ray
// came from, the material to shade with, and the radiance of a lamp.
class HitRecord {
 public:
  HitRecord()
      : t_(0.0f),
        hit_point_vec_(0.0f),
        normal_vec_(0.0f),
        u_(0.0f),
        v_(0.0f),
        front_face_(true),
        material_(nullptr),
        emission_(nullptr) {}

  float GetT() const { return t_; }
  const glm::vec3& GetHitPoint() const { return hit_point_vec_; }
  const glm::vec3& GetNormal() const { return normal_vec_; }
  float GetU() const { return u_; }
  float GetV() const { return v_; }
  // True unless a primitive called SetFaceNormal and the ray came from the
  // back of the surface.
  bool IsFrontFacing() const { return front_face_; }
  Material* GetMaterial() const { return material_; }
  // Null unless the surface is a lamp.
  const Texture* GetEmission() const { return emission_; }

  // Radiance the surface gives off here, toward where the ray came from. A
  // lamp emits from its front only, so the back of one gives off nothing.
  glm::vec3 Emitted() const {
    if (emission_ == nullptr || !front_face_) return glm::vec3(0.0f);
    return emission_->Value(u_, v_, hit_point_vec_);
  }

  void SetT(float t_float) { t_ = t_float; }
  void SetHitPoint(const glm::vec3& p_vec) { hit_point_vec_ = p_vec; }
  void SetNormal(const glm::vec3& n_vec) { normal_vec_ = n_vec; }
  void SetUV(float u, float v) {
    u_ = u;
    v_ = v;
  }
  void SetMaterial(Material* material) { material_ = material; }
  void SetEmission(const Texture* emission) { emission_ = emission; }

  // Every primitive writes all three together.
  void SetHit(float t_float, const glm::vec3& p_vec, const glm::vec3& n_vec) {
    t_ = t_float;
    hit_point_vec_ = p_vec;
    normal_vec_ = n_vec;
  }

  // Takes the surface's own (outward) normal, records which side the ray
  // arrived on, and stores the normal turned to face the ray.
  void SetFaceNormal(const Ray& r, const glm::vec3& outward_normal) {
    front_face_ = glm::dot(r.GetDirection(), outward_normal) < 0;
    normal_vec_ = front_face_ ? outward_normal : -outward_normal;
  }

 private:
  float t_;
  glm::vec3 hit_point_vec_;
  glm::vec3 normal_vec_;
  float u_;
  float v_;
  bool front_face_;
  Material* material_;
  const Texture* emission_;  // owned by the node that set it
};

#endif  // RAYTRACER_SRC_CORE_HIT_RECORD_H_
