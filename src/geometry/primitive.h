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
  NonhierBox(const glm::vec3& pos, double size);

  ~NonhierBox() override;
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;

 private:
  glm::vec3 pos_;
  double size_;

  // Built in the constructor, while the scene loads on one thread, so IsHit
  // reads it with no synchronisation. A per-call std::call_once cost ~20% of
  // render time here: libstdc++ routes every call through emulated TLS.
  std::unique_ptr<Primitive> mesh_;
};

#endif  // RAYTRACER_SRC_GEOMETRY_PRIMITIVE_H_
