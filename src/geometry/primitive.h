// Termm--Fall 2020

#ifndef RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
#define RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_

#include <glm/glm.hpp>
#include <mutex>

#include "core/hit_record.h"
#include "core/ray.h"

class Primitive {
 public:
  virtual ~Primitive();
  virtual bool IsHit(Ray& ray, float t0_float, float t1_float,
                     HitRecord& record);
};

class Sphere : public Primitive {
 public:
  ~Sphere() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;
};

class Cube : public Primitive {
 public:
  ~Cube() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;
};

class NonhierSphere : public Primitive {
 public:
  NonhierSphere(const glm::vec3& pos, double radius)
      : pos_(pos), radius_(radius) {}
  ~NonhierSphere() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;

 private:
  glm::vec3 pos_;
  double radius_;
};

class NonhierBox : public Primitive {
 public:
  NonhierBox(const glm::vec3& pos, double size) : pos_(pos), size_(size) {}

  ~NonhierBox() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;

 private:
  glm::vec3 pos_;
  double size_;

  // Built once on first intersection rather than once per ray.
  // call_once because every render thread may arrive here at the
  // same moment.
  mutable Primitive* mesh_ = nullptr;
  mutable std::once_flag mesh_once_;
};

#endif  // RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
