#ifndef RAYTRACER_SRC_GEOMETRY_SPHERE_UV_H_
#define RAYTRACER_SRC_GEOMETRY_SPHERE_UV_H_

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "render/sampling.h"

// Surface coordinates of a point `p` on the unit sphere at the origin
// (Ray Tracing: The Next Week, 4.3). v is the latitude, 0 at the bottom pole
// (-y) and 1 at the top; u is the longitude around +y, 0 at -x, 0.25 at +z,
// 0.5 at +x and 0.75 at -z, so the seam where u wraps from 1 to 0 faces -x.
//
// `p` must be unit length: pass (hit - centre) / radius, not the raw offset.
// The acos argument is clamped because a point a rounding error off the
// sphere would otherwise turn the pole into a NaN.
inline void SphereUV(const glm::vec3& p, float* u, float* v) {
  const float theta = std::acos(std::clamp(-p.y, -1.0f, 1.0f));
  const float phi = std::atan2(-p.z, p.x) + kPI;
  *u = phi / (2.0f * kPI);
  *v = theta / kPI;
}

#endif  // RAYTRACER_SRC_GEOMETRY_SPHERE_UV_H_
