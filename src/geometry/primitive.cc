// Termm--Fall 2020

#include "geometry/primitive.h"

#include <iostream>
#include <sstream>
#include <vector>

#include "core/ray.h"
#include "geometry/mesh.h"
#include "math/polyroots.h"

Primitive::~Primitive() {}

// If primitive is not defined
// default is no hit
bool Primitive::IsHit(Ray& ray, float t0_float, float t1_float,
                      HitRecord& record) {
  return false;
}

Sphere::~Sphere() {}
bool Sphere::IsHit(Ray& ray, float t0_float, float t1_float,
                   HitRecord& record) {
  // Was heap-allocating (and leaking) a NonhierSphere on every single
  // intersection test. The unit sphere never changes, so build it once.
  static const NonhierSphere kUnitSphere(glm::vec3(0.0, 0.0, 0.0), 1.0);
  return const_cast<NonhierSphere&>(kUnitSphere)
      .IsHit(ray, t0_float, t1_float, record);
}

Cube::~Cube() {}

bool Cube::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& record) {
  // Same fix as Sphere: one shared unit cube instead of one per ray.
  static NonhierBox unit_cube(glm::vec3(0.0, 0.0, 0.0), 1.0);
  return unit_cube.IsHit(ray, t0_float, t1_float, record);
}

NonhierSphere::~NonhierSphere() {}

// Use quadractic roots to calculate if a sphere is hit
bool NonhierSphere::IsHit(Ray& ray, float t0_float, float t1_float,
                          HitRecord& record) {
  glm::vec3 e_minus_c_vec = ray.GetOrigin() - pos_;
  glm::vec3 d_vec = ray.GetDirection();

  double a = static_cast<double>(dot(d_vec, d_vec));
  double b = static_cast<double>(2 * dot(d_vec, e_minus_c_vec));
  double c = (dot(e_minus_c_vec, e_minus_c_vec) - (radius_ * radius_));

  double roots[2];
  size_t num_roots = QuadraticRoots(a, b, c, roots);

  if (num_roots == 0) return false;

  // Nearest root in range, else the far one. That covers a transmitted ray
  // leaving through the far side, whose near root lies behind its origin,
  // without an inside/outside test that the surface epsilon cannot support.
  const double near_root =
      (num_roots == 1) ? roots[0] : glm::min(roots[0], roots[1]);
  const double far_root =
      (num_roots == 1) ? roots[0] : glm::max(roots[0], roots[1]);

  float t_float = static_cast<float>(near_root);
  if (t_float <= t0_float || t1_float <= t_float)
    t_float = static_cast<float>(far_root);

  if (t_float <= t0_float || t1_float <= t_float) {
    return false;
  }

  const glm::vec3 p_vec = ray.GetPointAtT(t_float);
  record.SetHit(t_float, p_vec, p_vec - pos_);
  return true;
}

NonhierBox::~NonhierBox() {}

bool NonhierBox::IsHit(Ray& ray, float t0_float, float t1_float,
                       HitRecord& record) {
  // This used to allocate a fresh 8-vertex, 12-triangle Mesh on EVERY
  // intersection test and leak it. With a BVH inside Mesh that would
  // also mean building a tree per ray. Build it once instead.
  std::call_once(mesh_once_, [this]() {
    std::vector<glm::vec3> vertices(8);
    // Bottom face
    vertices[0] = pos_ + glm::vec3(0.0f, 0.0f, 0.0f);
    vertices[1] = pos_ + glm::vec3(size_, 0.0f, 0.0f);
    vertices[2] = pos_ + glm::vec3(size_, 0.0f, size_);
    vertices[3] = pos_ + glm::vec3(0.0f, 0.0f, size_);
    // Top face
    vertices[4] = pos_ + glm::vec3(0.0f, size_, 0.0f);
    vertices[5] = pos_ + glm::vec3(size_, size_, 0.0f);
    vertices[6] = pos_ + glm::vec3(size_, size_, size_);
    vertices[7] = pos_ + glm::vec3(0.0f, size_, size_);

    std::vector<glm::vec3> tri_idx = {
        glm::vec3(0, 1, 2), glm::vec3(0, 2, 3), glm::vec3(0, 7, 4),
        glm::vec3(0, 3, 7), glm::vec3(0, 4, 5), glm::vec3(0, 5, 1),

        glm::vec3(6, 2, 1), glm::vec3(6, 1, 5), glm::vec3(6, 5, 4),
        glm::vec3(6, 4, 7), glm::vec3(6, 7, 3), glm::vec3(6, 3, 2)};

    mesh_ = new Mesh(vertices, tri_idx);
  });

  return mesh_->IsHit(ray, t0_float, t1_float, record);
}
