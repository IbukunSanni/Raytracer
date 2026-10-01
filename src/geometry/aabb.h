// Raytracer -- axis-aligned bounding box
//
// The building block of the BVH.  An AABB is the cheapest useful
// "does the ray go anywhere near this?" test: if a ray misses the box,
// it misses everything inside the box, so a whole subtree can be
// skipped without touching a single triangle.
//
// Everything mechanical (growing boxes, centroids, surface area) is
// provided.  The intersection test itself is yours.

#ifndef RAYTRACER_SRC_GEOMETRY_AABB_H_
#define RAYTRACER_SRC_GEOMETRY_AABB_H_

#include <algorithm>
#include <glm/glm.hpp>
#include <limits>
#include <utility>

#include "core/ray.h"

struct AABB {
  glm::vec3 min_vec;
  glm::vec3 max_vec;

  // An empty box: deliberately inverted, so the first Expand() call
  // snaps it onto the real data.
  AABB()
      : min_vec(std::numeric_limits<float>::max()),
        max_vec(-std::numeric_limits<float>::max()) {}

  AABB(const glm::vec3& lo, const glm::vec3& hi) : min_vec(lo), max_vec(hi) {}

  bool IsEmpty() const { return min_vec.x > max_vec.x; }

  // Grow to contain a point.
  void Expand(const glm::vec3& p) {
    min_vec = glm::min(min_vec, p);
    max_vec = glm::max(max_vec, p);
  }

  // Grow to contain another box.
  void Expand(const AABB& other) {
    if (other.IsEmpty()) return;
    min_vec = glm::min(min_vec, other.min_vec);
    max_vec = glm::max(max_vec, other.max_vec);
  }

  glm::vec3 Extent() const {
    return IsEmpty() ? glm::vec3(0.0f) : (max_vec - min_vec);
  }

  glm::vec3 Centroid() const { return 0.5f * (min_vec + max_vec); }

  // Index of the longest axis: 0 = x, 1 = y, 2 = z.  The usual choice
  // for a median split, because splitting the long axis keeps child
  // boxes closer to cubes and so keeps their surface area down.
  int LongestAxis() const {
    const glm::vec3 e = Extent();
    if (e.x >= e.y && e.x >= e.z) return 0;
    return (e.y >= e.z) ? 1 : 2;
  }

  // Needed later if you upgrade the split heuristic to SAH.  Not used
  // by a median split.
  float SurfaceArea() const {
    const glm::vec3 e = Extent();
    return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
  }

  // Slab test: true if the ray overlaps this box anywhere in
  // [t0_float, t1_float].  The box is the intersection of three slabs,
  // one per axis; each slab narrows the range to the part of the ray
  // inside it, and the ray hits the box if anything is left.
  //
  // ray_inv_dir is 1 / direction, computed once per ray by the caller.
  // A zero direction component gives +-inf, which the comparisons
  // handle; an origin exactly on a slab plane gives NaN, whose
  // comparisons are all false, so that axis is skipped and the test
  // errs toward a hit.
  //
  // The direction must NOT be normalized: t has to mean the same thing
  // here as in the triangle test.
  bool Hit(const glm::vec3& ray_origin, const glm::vec3& ray_inv_dir,
           float t0_float, float t1_float) const {
    for (int axis = 0; axis < 3; ++axis) {
      float t_near = (min_vec[axis] - ray_origin[axis]) * ray_inv_dir[axis];
      float t_far = (max_vec[axis] - ray_origin[axis]) * ray_inv_dir[axis];
      if (ray_inv_dir[axis] < 0.0f) std::swap(t_near, t_far);

      if (t_near > t0_float) t0_float = t_near;
      if (t_far < t1_float) t1_float = t_far;
      // Strict < so a flat box (min == max on one axis) still counts as hit.
      if (t1_float < t0_float) return false;
    }
    return true;
  }
};

#endif  // RAYTRACER_SRC_GEOMETRY_AABB_H_
