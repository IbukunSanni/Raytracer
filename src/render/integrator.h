// Raytracer -- the path tracer: one camera ray in, radiance out

#ifndef RAYTRACER_SRC_RENDER_INTEGRATOR_H_
#define RAYTRACER_SRC_RENDER_INTEGRATOR_H_

#include <glm/glm.hpp>
#include <list>
#include <vector>

#include "core/ray.h"
#include "geometry/primitive.h"
#include "render/environment.h"
#include "render/frame_stats.h"
#include "render/sampling.h"
#include "scene/emitters.h"
#include "scene/light.h"
#include "scene/scene_node.h"

// Trace one path: bounce until it escapes, dies to roulette, or hits the
// depth cap, accumulating radiance weighted by the throughput carried so
// far. One loop iteration is one ray cast; `counts` tallies them by kind.
//
// max_depth is a safety valve for pathological geometry -- a hall of
// mirrors, or light trapped in a box. Russian roulette is the real
// termination and is unbiased; the cap is not, so it should almost never
// fire.
//
// `sampled`, in world space, is normally empty. When it is not, every
// non-specular bounce draws half the time toward one of its shapes and half
// from its material, weighted by the mixture's density. Every direction the
// material can take keeps a nonzero chance, so the image stays unbiased.
glm::vec3 RayTraceRgb(SceneNode* root,
                      Ray ray,  // by value: the loop advances it
                      Rng& rng, const Environment& environment,
                      const std::list<Light*>& lights,
                      const SampledShapes& sampled, int max_depth,
                      RayCounts& counts);

#endif  // RAYTRACER_SRC_RENDER_INTEGRATOR_H_
