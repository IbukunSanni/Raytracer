// Termm--Fall 2020

#ifndef RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
#define RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_

#include <glm/glm.hpp>
#include <memory>

#include "core/hit_record.h"
#include "core/ray.h"

class Primitive {
 public:
  virtual ~Primitive();
  virtual bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit);
};

class Sphere : public Primitive {
 public:
  ~Sphere() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
};

class Cube : public Primitive {
 public:
  ~Cube() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
};

class NonhierSphere : public Primitive {
 public:
  NonhierSphere(const glm::vec3& pos, double radius)
      : pos_(pos), pos1_(pos), radius_(radius) {}
  // A sphere that moves in a straight line during the shutter, from `pos`
  // at time 0 to `pos1` at time 1 (Ray Tracing: The Next Week, 2).
  NonhierSphere(const glm::vec3& pos, const glm::vec3& pos1, double radius)
      : pos_(pos), pos1_(pos1), radius_(radius), moving_(true) {}
  ~NonhierSphere() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;

 private:
  // The centre at `time`. A still sphere returns pos_ itself rather than
  // pos_ + time * 0, so its arithmetic is exactly what it was before.
  glm::vec3 CentreAt(float time) const {
    return moving_ ? pos_ + time * (pos1_ - pos_) : pos_;
  }

  glm::vec3 pos_;
  glm::vec3 pos1_;
  double radius_;
  bool moving_ = false;
};

class NonhierBox : public Primitive {
 public:
  NonhierBox(const glm::vec3& pos, double size);

  ~NonhierBox() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;

 private:
  glm::vec3 pos_;
  double size_;

  // Built in the constructor, while the scene loads on one thread, so IsHit
  // reads it with no synchronisation. A per-call std::call_once cost ~20% of
  // render time here: libstdc++ routes every call through emulated TLS.
  std::unique_ptr<Primitive> mesh_;
};

#endif  // RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
