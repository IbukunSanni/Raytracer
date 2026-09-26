// Raytracer -- aperture shapes
//
// An out-of-focus highlight is a picture of the aperture, so the shape of
// the lens opening is the shape of the bokeh. Every shape here is a mask
// inscribed in the unit circle, sampled uniformly by rejection, and scaled
// by the lens radius at the call site.
//
// These live apart from the disk sampler deliberately. SampleUnitDisk is
// also the body of cosine-weighted hemisphere sampling, so a shape put
// there would silently retune every diffuse BSDF; the aperture gets its
// own primitive instead, and the camera is the only thing that reads it.

#ifndef RAYTRACER_SRC_RENDER_APERTURE_H_
#define RAYTRACER_SRC_RENDER_APERTURE_H_

#include <cstddef>
#include <glm/glm.hpp>
#include <string>

#include "render/sampling.h"

// The lens openings a scene can ask for. kDisk is the physical default and
// the only one that is not a cut-out.
enum class ApertureShape { kDisk, kHexagon, kStar, kHeart, kCrown };

// Vertices are listed in order around the boundary, already scaled so the
// farthest one sits on the unit circle. A shape wider than that circle
// would outgrow the lens radius the scene asked for.
//
// Corners at (+-1, 0), so the hexagon has a flat top and bottom at the
// apothem sqrt(3)/2, which is the orientation a bladed iris settles into.
inline const glm::vec2 kHexagonVerts[] = {
    {+1.0000000f, +0.0000000f}, {+0.5000000f, +0.8660254f},
    {-0.5000000f, +0.8660254f}, {-1.0000000f, +0.0000000f},
    {-0.5000000f, -0.8660254f}, {+0.5000000f, -0.8660254f},
};

// A five-pointed star: ten vertices alternating between the unit circle and
// an inner circle at cos(2pi/5) / cos(pi/5), which is the ratio that puts
// the valleys exactly where a pentagram's edges would cross.
inline const glm::vec2 kStarVerts[] = {
    {+0.0000000f, +1.0000000f}, {-0.2245140f, +0.3090170f},
    {-0.9510565f, +0.3090170f}, {-0.3632713f, -0.1180340f},
    {-0.5877853f, -0.8090170f}, {+0.0000000f, -0.3819660f},
    {+0.5877853f, -0.8090170f}, {+0.3632713f, -0.1180340f},
    {+0.9510565f, +0.3090170f}, {+0.2245140f, +0.3090170f},
};

// The Kingdom Hearts crown: three spikes over a base that dips at the
// centre. Eight vertices is as coarse as the silhouette can get and still
// be read as a crown at the size a blur circle actually lands on film.
inline const glm::vec2 kCrownVerts[] = {
    {-0.8762159f, -0.4819187f}, {-0.8762159f, +0.3942972f},
    {-0.3680107f, -0.0438108f}, {+0.0000000f, +0.6834484f},
    {+0.3680107f, -0.0438108f}, {+0.8762159f, +0.3942972f},
    {+0.8762159f, -0.4819187f}, {+0.0000000f, -0.2628648f},
};

// Even-odd crossing test, which needs no winding order and handles the
// star's and the crown's concave notches without special-casing them. The
// vertex count comes from the array, so it cannot fall out of step with it.
template <std::size_t Count>
inline bool InsidePolygon(const glm::vec2 (&poly)[Count], const glm::vec2& p) {
  bool inside = false;
  for (std::size_t i = 0, j = Count - 1; i < Count; j = i++) {
    const glm::vec2& a = poly[i];
    const glm::vec2& b = poly[j];
    // Half-open in y, so a vertex exactly at p.y is counted once, not twice.
    if ((a.y > p.y) != (b.y > p.y) &&
        p.x < a.x + (b.x - a.x) * (p.y - a.y) / (b.y - a.y)) {
      inside = !inside;
    }
  }
  return inside;
}

// The sextic (x^2 + y^2 - 1)^3 <= x^2 y^3, divided down until its farthest
// point -- the outer shoulder of a lobe, at radius 1.424548 -- lands on the
// unit circle, with the last digit rounded up so no draw can escape it.
inline bool InsideHeart(const glm::vec2& p) {
  constexpr float kHeartExtent = 1.4246f;
  const float x = p.x * kHeartExtent;
  const float y = p.y * kHeartExtent;
  const float t = x * x + y * y - 1.0f;
  return t * t * t - x * x * y * y * y <= 0.0f;
}

inline bool InsideAperture(ApertureShape shape, const glm::vec2& p) {
  switch (shape) {
    case ApertureShape::kDisk:
      return p.x * p.x + p.y * p.y <= 1.0f;
    case ApertureShape::kHexagon:
      return InsidePolygon(kHexagonVerts, p);
    case ApertureShape::kStar:
      return InsidePolygon(kStarVerts, p);
    case ApertureShape::kHeart:
      return InsideHeart(p);
    case ApertureShape::kCrown:
      return InsidePolygon(kCrownVerts, p);
  }
  return false;
}

// A uniform point on the aperture, in units of the lens radius.
inline glm::vec2 SampleAperture(ApertureShape shape, Rng& rng) {
  // The disk has a sampler already, and reusing it keeps a scene that asks
  // for nothing rendering the same draws it always did.
  if (shape == ApertureShape::kDisk) return SampleUnitDisk(rng);

  // The narrowest cut-out here, the crown, covers 0.27 of the square, so a
  // draw fails almost three times in four and the cap has to sit far above
  // the disk's: at 128 attempts the fallback arrives once in 10^17 calls.
  const int max_attempts = 128;

  for (int i = 0; i < max_attempts; ++i) {
    const glm::vec2 p(rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f));
    if (InsideAperture(shape, p)) return p;
  }

  // The centre is inside every shape here, so the fallback is at worst a
  // sample in the wrong place, never one outside the mask.
  return glm::vec2(0.0f, 0.0f);
}

// Parse the name a scene writes. Returns false on an unknown one so the
// caller can quote the offender back.
inline bool ApertureShapeFromName(const std::string& name,
                                  ApertureShape* shape) {
  struct Named {
    const char* name;
    ApertureShape shape;
  };

  // A table rather than a switch, because the thing being matched is a
  // string. One row per shape, so a new cut-out is one line here.
  static const Named kNames[] = {
      {"disk", ApertureShape::kDisk},   {"hexagon", ApertureShape::kHexagon},
      {"star", ApertureShape::kStar},   {"heart", ApertureShape::kHeart},
      {"crown", ApertureShape::kCrown},
  };

  for (const Named& entry : kNames) {
    if (name == entry.name) {
      *shape = entry.shape;
      return true;
    }
  }
  return false;
}

#endif  // RAYTRACER_SRC_RENDER_APERTURE_H_
