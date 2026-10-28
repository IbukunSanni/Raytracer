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

// A flat parallelogram: corner q and the two edges u and v leaving it. A hit
// reports the point's coordinates along the edges, (alpha, beta), as its
// (u, v), and a normal turned to face the ray, so either side shades.
class Quad : public Primitive {
 public:
  Quad(const glm::vec3& q, const glm::vec3& u, const glm::vec3& v);

  ~Quad() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;

 private:
  // Derived once here, and held in double so a hit far down the ray still
  // lands on the plane where it should.
  glm::dvec3 q_;
  glm::dvec3 u_;
  glm::dvec3 v_;
  glm::dvec3 normal_;  // unit, along cross(u, v)
  double d_;           // the plane: dot(normal_, p) == d_
  glm::dvec3 w_;       // cross(u, v) / |cross(u, v)|^2, gives alpha and beta
};

#endif  // RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
