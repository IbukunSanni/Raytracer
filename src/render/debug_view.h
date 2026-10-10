// Raytracer -- debug views: draw one quantity per pixel instead of shading
//
// A wrong colour in a shaded render has a dozen possible causes. A wrong
// normal or UV has one or two, and seen as an image it is usually obvious:
// a seam, a flipped axis, a face pointing the wrong way. RT_VIEW swaps the
// path tracer for a single primary-hit lookup that writes that quantity as
// the pixel's colour.
//
// The value is written as-is: GetToneMap() returns no tone map and no sRGB
// transfer while a view is active, so a pixel read back from the PNG is
// the quantity itself, scaled to [0, 255].

#ifndef RAYTRACER_SRC_RENDER_DEBUG_VIEW_H_
#define RAYTRACER_SRC_RENDER_DEBUG_VIEW_H_

#include <glm/glm.hpp>

#include "core/ray.h"
#include "render/frame_stats.h"
#include "scene/scene_node.h"

// Add a view by adding an enumerator, its name in ViewName(), and its case
// in TraceView().
enum class RenderView { kShaded, kNormal, kAlbedo, kUV };

// RT_VIEW=shaded|normal|albedo|uv, read once. Unset means shaded. An unknown
// value exits rather than shading, so a debug render never silently turns into
// a normal one.
RenderView ActiveView();
const char* ViewName(RenderView view);

// The colour `view` draws for one camera ray. Only the primary hit is read:
// no bounces, no lights, no environment, and a miss is black.
//
// kNormal: the hit record's normal, normalised, as (n + 1) / 2, so +x is
// red, +y green and +z blue, in world space. It is the normal exactly as
// the scene graph hands it back, so a mesh's normals, which Mesh flips to
// face the ray, show the side the ray arrived from.
//
// kAlbedo: Material::Albedo() at the hit -- the texture's value for a
// Lambertian, kd for Blinn-Phong, the tint of a mirror or metal, white for
// glass. A hit with no material is magenta, which no albedo should be.
//
// kUV: the hit's (u, v) as (red, green, 0), so u grows to the red and v to
// the green. A surface that sets no (u, v) reads (0, 0) and draws black;
// so does a miss. A sphere shows a red ramp around it with a hard edge at
// the seam, where u wraps from 1 back to 0.
glm::vec3 TraceView(SceneNode* root, Ray ray, RenderView view,
                    RayCounts& counts);

#endif  // RAYTRACER_SRC_RENDER_DEBUG_VIEW_H_
