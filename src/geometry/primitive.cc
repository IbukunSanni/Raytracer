#include "geometry/primitive.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "core/ray.h"
#include "geometry/mesh.h"
#include "geometry/sphere_uv.h"
#include "math/polyroots.h"
#include "render/sampling.h"

Primitive::~Primitive() {}

// If primitive is not defined
// default is no hit
bool Primitive::IsHit(Ray& ray, float t0_float, float t1_float,
                      HitRecord& hit) {
  return false;
}

AABB Primitive::Bounds() const { return AABB::Infinite(); }

Sphere::~Sphere() {}
bool Sphere::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  // The unit sphere never changes, so it is built once, not per test.
  static const NonhierSphere kUnitSphere(glm::vec3(0.0, 0.0, 0.0), 1.0);
  return const_cast<NonhierSphere&>(kUnitSphere)
      .IsHit(ray, t0_float, t1_float, hit);
}

AABB Sphere::Bounds() const { return AABB(glm::vec3(-1.0f), glm::vec3(1.0f)); }

Cube::~Cube() {}

bool Cube::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  // One shared unit cube instead of one per ray.
  static NonhierBox unit_cube(glm::vec3(0.0, 0.0, 0.0), 1.0);
  return unit_cube.IsHit(ray, t0_float, t1_float, hit);
}

// The unit cube IsHit tests against, from the origin to (1, 1, 1).
AABB Cube::Bounds() const { return AABB(glm::vec3(0.0f), glm::vec3(1.0f)); }

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

// The union of the boxes at both ends of the shutter; a still sphere has
// pos1_ == pos_, so it gets its own box.
AABB NonhierSphere::Bounds() const {
  const glm::vec3 r(radius_);
  AABB box(pos_ - r, pos_ + r);
  box.Expand(AABB(pos1_ - r, pos1_ + r));
  return box;
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

AABB NonhierBox::Bounds() const { return AABB(pos_, pos_ + glm::vec3(size_)); }

Quad::Quad(const glm::vec3& q, const glm::vec3& u, const glm::vec3& v)
    : q_(q), u_(u), v_(v) {
  const glm::dvec3 n_vec = cross(u_, v_);
  normal_ = normalize(n_vec);
  d_ = dot(normal_, q_);
  w_ = n_vec / dot(n_vec, n_vec);
  area_ = length(n_vec);
}

Quad::~Quad() {}

// The plane first, then whether the point on it lies inside both edges.
bool Quad::Intersect(const glm::dvec3& e_vec, const glm::dvec3& d_vec,
                     double t0, double t1, double* t, double* alpha,
                     double* beta) const {
  // A ray parallel to the plane divides by zero; the infinity or NaN that
  // gives fails the range test, so it needs no case of its own.
  const double denom = dot(normal_, d_vec);
  *t = (d_ - dot(normal_, e_vec)) / denom;
  if (!(*t > t0 && *t < t1)) return false;

  const glm::dvec3 planar_vec = e_vec + *t * d_vec - q_;
  *alpha = dot(w_, cross(planar_vec, v_));
  *beta = dot(w_, cross(u_, planar_vec));
  return *alpha >= 0.0 && *alpha <= 1.0 && *beta >= 0.0 && *beta <= 1.0;
}

bool Quad::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  const glm::dvec3 e_vec(ray.GetOrigin());
  const glm::dvec3 d_vec(ray.GetDirection());

  double t, alpha, beta;
  if (!Intersect(e_vec, d_vec, t0_float, t1_float, &t, &alpha, &beta))
    return false;

  // The plane's own normal goes in; the hit records which side the ray came
  // from and keeps the normal turned to face it, so either side shades.
  const glm::vec3 outward = glm::vec3(normal_);
  hit.SetHit(static_cast<float>(t), glm::vec3(e_vec + t * d_vec), outward);
  hit.SetFaceNormal(ray, outward);
  hit.SetUV(static_cast<float>(alpha), static_cast<float>(beta));
  return true;
}

// A patch dA of the quad fills dw = cos * dA / distance^2 of the sky around
// `origin`, so a density of 1 / area over the quad's surface is
// distance^2 / (cos * area) over solid angle.
float Quad::Pdf(const glm::vec3& origin, const glm::vec3& direction) const {
  const glm::dvec3 d_vec(direction);

  double t, alpha, beta;
  if (!Intersect(glm::dvec3(origin), d_vec, 0.0,
                 std::numeric_limits<double>::infinity(), &t, &alpha, &beta)) {
    return 0.0f;
  }

  const double length_squared = dot(d_vec, d_vec);
  const double distance_squared = t * t * length_squared;
  const double cosine =
      std::fabs(dot(d_vec, normal_)) / std::sqrt(length_squared);
  return static_cast<float>(distance_squared / (cosine * area_));
}

glm::vec3 Quad::SampleToward(const glm::vec3& origin, Rng& rng) const {
  const double alpha = rng.Next();
  const double beta = rng.Next();
  const glm::dvec3 p_vec = q_ + alpha * u_ + beta * v_;
  return glm::vec3(normalize(p_vec - glm::dvec3(origin)));
}

Quad Quad::Transformed(const glm::mat4& m) const {
  return Quad(glm::vec3(m * glm::vec4(glm::vec3(q_), 1.0f)),
              glm::vec3(m * glm::vec4(glm::vec3(u_), 0.0f)),
              glm::vec3(m * glm::vec4(glm::vec3(v_), 0.0f)));
}

// Its four corners, padded so a quad lying in an axis plane is not flat.
AABB Quad::Bounds() const {
  AABB box;
  box.Expand(glm::vec3(q_));
  box.Expand(glm::vec3(q_ + u_));
  box.Expand(glm::vec3(q_ + v_));
  box.Expand(glm::vec3(q_ + u_ + v_));
  return box.Padded(1e-4f);
}

Box::Box(const glm::vec3& a, const glm::vec3& b)
    : lo_(glm::min(a, b)), hi_(glm::max(a, b)) {}

Box::~Box() = default;

// The ray's span between each pair of faces, intersected: it enters at the
// latest entry and leaves at the earliest exit. The hit is the entry if that
// lies in (t0, t1), else the exit: a ray starting inside hits the face it
// leaves by.
bool Box::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
  const glm::dvec3 e_vec(ray.GetOrigin());
  const glm::dvec3 d_vec(ray.GetDirection());

  double t_enter = -std::numeric_limits<double>::infinity();
  double t_leave = std::numeric_limits<double>::infinity();
  int enter_axis = -1;
  int leave_axis = -1;
  for (int axis = 0; axis < 3; ++axis) {
    // Divided, not multiplied by 1 / d, so t is the correctly rounded
    // crossing of the face plane. A ray parallel to this pair gives +-inf (a
    // miss from outside, no limit from inside), or NaN from an origin on a
    // face plane, which every comparison below ignores.
    double t_lo = (lo_[axis] - e_vec[axis]) / d_vec[axis];
    double t_hi = (hi_[axis] - e_vec[axis]) / d_vec[axis];
    if (t_lo > t_hi) std::swap(t_lo, t_hi);
    if (t_lo > t_enter) {
      t_enter = t_lo;
      enter_axis = axis;
    }
    if (t_hi < t_leave) {
      t_leave = t_hi;
      leave_axis = axis;
    }
  }
  // <=, so a ray grazing an edge or a corner counts as a hit.
  if (!(t_enter <= t_leave)) return false;

  double t;
  int axis;
  if (t_enter > t0_float && t_enter < t1_float) {
    t = t_enter;
    axis = enter_axis;
  } else if (t_leave > t0_float && t_leave < t1_float) {
    t = t_leave;
    axis = leave_axis;
  } else {
    return false;
  }
  if (axis < 0) return false;  // a zero direction crosses no face

  glm::dvec3 n_vec(0.0);
  n_vec[axis] = d_vec[axis] > 0.0 ? -1.0 : 1.0;
  const glm::dvec3 p_vec = e_vec + t * d_vec;
  hit.SetHit(static_cast<float>(t), glm::vec3(p_vec), glm::vec3(n_vec));
  return true;
}

// Exactly its faces, padded only on an axis where it has no thickness, so a
// flat box still has volume.
AABB Box::Bounds() const {
  return AABB(glm::vec3(lo_), glm::vec3(hi_)).Padded(1e-4f);
}

ConstantMedium::~ConstantMedium() {}

// Where the ray enters and leaves the boundary, then a free-flight distance
// drawn from e^(-density * d): a hit if it falls short of the exit.
bool ConstantMedium::IsHit(Ray& ray, float t0_float, float t1_float,
                           HitRecord& hit) {
  constexpr float kInf = std::numeric_limits<float>::infinity();

  // Over the whole line, not [t0, t1], so a ray starting inside still
  // finds the entry behind it and knows it is inside.
  HitRecord enter, leave;
  if (!boundary_->IsHit(ray, -kInf, kInf, enter)) return false;
  if (!boundary_->IsHit(ray, enter.GetT() + 0.0001f, kInf, leave)) return false;

  const float t_enter = std::max(enter.GetT(), t0_float);
  const float t_leave = std::min(leave.GetT(), t1_float);
  if (t_enter >= t_leave) return false;

  assert(ray.GetRng() && "a ray that can reach a medium carries an Rng");
  const double ray_length = glm::length(glm::dvec3(ray.GetDirection()));
  const double inside = (t_leave - t_enter) * ray_length;
  // 1 - u lies in (0, 1], so the log is finite.
  const double depth = neg_inv_density_ * std::log(1.0 - ray.GetRng()->Next());
  if (depth > inside) return false;  // passes through

  const float t = t_enter + static_cast<float>(depth / ray_length);
  hit.SetHit(t, ray.GetPointAtT(t), glm::vec3(1.0f, 0.0f, 0.0f));
  return true;
}

// Every stop lies between the boundary's entry and exit, so inside it.
AABB ConstantMedium::Bounds() const { return boundary_->Bounds(); }
