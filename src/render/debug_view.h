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
// in TraceView(). `uv` belongs here once HitRecord carries a UV (step 9,
// piece 3), and `albedo` once materials read a texture (piece 1b).
enum class RenderView { kShaded, kNormal };

// RT_VIEW=shaded|normal, read once. Unset means shaded. An unknown value
// exits rather than shading, so a debug render never silently turns into a
// normal one.
RenderView ActiveView();
const char* ViewName(RenderView view);

// The colour `view` draws for one camera ray. Only the primary hit is read:
// no bounces, no lights, no environment, and a miss is black.
//
// kNormal: the hit record's normal, normalised, as (n + 1) / 2, so +x is
// red, +y green and +z blue, in world space. It is the normal exactly as
// the scene graph hands it back, so a mesh's normals, which Mesh flips to
// face the ray, show the side the ray arrived from.
glm::vec3 TraceView(SceneNode* root, Ray ray, RenderView view,
                    RayCounts& counts);

#endif  // RAYTRACER_SRC_RENDER_DEBUG_VIEW_H_
