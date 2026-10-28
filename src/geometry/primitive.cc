#include "geometry/primitive.h"

#include <iostream>
#include <sstream>
#include <vector>

#include "core/ray.h"
#include "geometry/mesh.h"
#include "geometry/sphere_uv.h"
#include "math/polyroots.h"

Primitive::~Primitive() {}

// If primitive is not defined
// default is no hit
bool Primitive::IsHit(Ray& ray, float t0_float, float t1_float,
                      HitRecord& hit) {
  return false;
}

Sphere::~Sphere() {}
bool Sphere::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  // Was heap-allocating (and leaking) a NonhierSphere on every single
  // intersection test. The unit sphere never changes, so build it once.
  static const NonhierSphere kUnitSphere(glm::vec3(0.0, 0.0, 0.0), 1.0);
  return const_cast<NonhierSphere&>(kUnitSphere)
      .IsHit(ray, t0_float, t1_float, hit);
}

Cube::~Cube() {}

bool Cube::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  // One shared unit cube instead of one per ray.
  static NonhierBox unit_cube(glm::vec3(0.0, 0.0, 0.0), 1.0);
  return unit_cube.IsHit(ray, t0_float, t1_float, hit);
}

NonhierSphere::~NonhierSphere() {}

// Use quadractic roots to calculate if a sphere is hit
bool NonhierSphere::IsHit(Ray& ray, float t0_float, float t1_float,
                          HitRecord& hit) {
  const glm::vec3 centre = CentreAt(ray.GetTime());
  const glm::dvec3 e_minus_c_vec =
      glm::dvec3(ray.GetOrigin()) - glm::dvec3(centre);
  const glm::dvec3 d_vec(ray.GetDirection());

  double a = dot(d_vec, d_vec);
  double b = 2.0 * dot(d_vec, e_minus_c_vec);
  double c = dot(e_minus_c_vec, e_minus_c_vec) - (radius_ * radius_);

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

  // The point in double too, rounded to float once at the end.
  const glm::vec3 p_vec(glm::dvec3(ray.GetOrigin()) +
                        static_cast<double>(t_float) * d_vec);
  hit.SetHit(t_float, p_vec, p_vec - centre);

  // In the sphere's own space, so a texture turns with the sphere.
  float u, v;
  SphereUV((p_vec - centre) / static_cast<float>(radius_), &u, &v);
  hit.SetUV(u, v);
  return true;
}

// The eight corners and twelve triangles are built once here, not per ray.
NonhierBox::NonhierBox(const glm::vec3& pos, double size)
    : pos_(pos), size_(size) {
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

  mesh_ = std::make_unique<Mesh>(vertices, tri_idx);
}

NonhierBox::~NonhierBox() = default;

bool NonhierBox::IsHit(Ray& ray, float t0_float, float t1_float,
                       HitRecord& hit) {
  return mesh_->IsHit(ray, t0_float, t1_float, hit);
}

Quad::Quad(const glm::vec3& q, const glm::vec3& u, const glm::vec3& v)
    : q_(q), u_(u), v_(v) {
  const glm::dvec3 n_vec = cross(u_, v_);
  normal_ = normalize(n_vec);
  d_ = dot(normal_, q_);
  w_ = n_vec / dot(n_vec, n_vec);
}

Quad::~Quad() {}

// The plane first, then whether the point on it lies inside both edges.
bool Quad::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  const glm::dvec3 e_vec(ray.GetOrigin());
  const glm::dvec3 d_vec(ray.GetDirection());

  // A ray parallel to the plane divides by zero; the infinity or NaN that
  // gives fails the range test, so it needs no case of its own.
  const double denom = dot(normal_, d_vec);
  const double t = (d_ - dot(normal_, e_vec)) / denom;
  if (!(t > t0_float && t < t1_float)) return false;

  const glm::dvec3 p_vec = e_vec + t * d_vec;
  const glm::dvec3 planar_vec = p_vec - q_;
  const double alpha = dot(w_, cross(planar_vec, v_));
  const double beta = dot(w_, cross(u_, planar_vec));
  if (!(alpha >= 0.0 && alpha <= 1.0 && beta >= 0.0 && beta <= 1.0))
    return false;

  const glm::dvec3 n_vec = denom > 0.0 ? -normal_ : normal_;
  hit.SetHit(static_cast<float>(t), glm::vec3(p_vec), glm::vec3(n_vec));
  hit.SetUV(static_cast<float>(alpha), static_cast<float>(beta));
  return true;
}
