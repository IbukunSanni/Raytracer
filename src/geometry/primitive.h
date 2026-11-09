// Termm--Fall 2020

#ifndef RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
#define RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_

#include <glm/glm.hpp>
#include <memory>
#include <utility>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/aabb.h"

class Rng;

class Primitive {
 public:
  virtual ~Primitive();
  virtual bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit);

  // A box holding every hit, in the primitive's own space. Infinite unless
  // a subclass knows better, so nothing without bounds is ever culled.
  virtual AABB Bounds() const;
};

class Sphere : public Primitive {
 public:
  ~Sphere() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
  AABB Bounds() const override;
};

class Cube : public Primitive {
 public:
  ~Cube() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
  AABB Bounds() const override;
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
  AABB Bounds() const override;

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
  AABB Bounds() const override;

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
  AABB Bounds() const override;

  float Area() const { return static_cast<float>(area_); }

  // Solid-angle density, seen from `origin`, of the direction `direction`
  // under SampleToward: distance^2 / (cos * area), or 0 if the ray misses.
  // `direction` need not be unit length. The density ignores which side of
  // the quad the ray arrives on.
  float Pdf(const glm::vec3& origin, const glm::vec3& direction) const;

  // A unit direction from `origin` to a point uniform over the quad's area.
  glm::vec3 SampleToward(const glm::vec3& origin, Rng& rng) const;

  // This quad carried through `m`. An affine map keeps a parallelogram a
  // parallelogram and scales all of its area by one factor, so a point uniform
  // here stays uniform there. A mirror reverses cross(u, v), so the result's
  // normal does not say which side of the original was its front.
  Quad Transformed(const glm::mat4& m) const;

 private:
  // The ray e + t * d against the plane and both edges, for t in (t0, t1).
  bool Intersect(const glm::dvec3& e_vec, const glm::dvec3& d_vec, double t0,
                 double t1, double* t, double* alpha, double* beta) const;

  // Derived once here, and held in double so a hit far down the ray still
  // lands on the plane where it should.
  glm::dvec3 q_;
  glm::dvec3 u_;
  glm::dvec3 v_;
  glm::dvec3 normal_;  // unit, along cross(u, v)
  double d_;           // the plane: dot(normal_, p) == d_
  glm::dvec3 w_;       // cross(u, v) / |cross(u, v)|^2, gives alpha and beta
  double area_;        // |cross(u, v)|
};

// An axis-aligned box between two corners, hit by one slab test. Its normal
// is turned to face the ray, and a ray starting inside hits the face it
// leaves by; a medium's boundary depends on that exit hit. It reports no
// (u, v), so it suits untextured materials and medium boundaries until a
// textured box needs the faces' coordinates.
class Box : public Primitive {
 public:
  // Any two opposite corners, in either order.
  Box(const glm::vec3& a, const glm::vec3& b);

  ~Box() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
  AABB Bounds() const override;

 private:
  // In double, so a hit far down the ray still lands on its face.
  glm::dvec3 lo_;
  glm::dvec3 hi_;
};

// Smoke or fog filling a closed boundary at a constant density. A ray
// passing through stops at a random depth, more likely the denser it is,
// and reports a hit there; its node's material scatters it. Density is per
// unit length in the medium's own space, so scaling its node thins it.
class ConstantMedium : public Primitive {
 public:
  ConstantMedium(std::unique_ptr<Primitive> boundary, double density)
      : boundary_(std::move(boundary)), neg_inv_density_(-1.0 / density) {}

  ~ConstantMedium() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
  AABB Bounds() const override;

 private:
  std::unique_ptr<Primitive> boundary_;
  double neg_inv_density_;  // -1 / density, so a depth is this * log(1 - u)
};

#endif  // RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
