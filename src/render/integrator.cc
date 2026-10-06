// Raytracer -- the path tracer: one camera ray in, radiance out

#include "render/integrator.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/hit_record.h"
#include "core/stats.h"
#include "scene/material.h"

static const float kMaxT =
    std::numeric_limits<float>::max();  // unbounded ray length
static const int kRrStartDepth = 3;     // bounces taken before roulette begins

// A hit point pushed clear of the surface along dir. The offset scales with
// the point's own magnitude because kEpsilon is absolute: past a few hundred
// units it is under one float ULP, and an unscaled nudge rounds away to zero.
static glm::vec3 OffsetFromSurface(const glm::vec3& hit_point,
                                   const glm::vec3& dir) {
  const float scale = std::max({std::fabs(hit_point.x), std::fabs(hit_point.y),
                                std::fabs(hit_point.z), 1.0f});
  return hit_point + dir * (kEpsilon * scale);
}

glm::vec3 RayTraceRgb(SceneNode* root, Ray ray, Rng& rng,
                      const Environment& environment,
                      const std::list<Light*>& lights, int max_depth,
                      RayCounts& counts) {
  glm::vec3 radiance(0.0f);
  glm::vec3 throughput(1.0f);

  for (int bounces = 0;; ++bounces) {
    // kEpsilon as t_min: ignore hits right at the ray origin.
    HitRecord hit;
    if (rt::stats::kEnabled) ++(bounces == 0 ? counts.primary : counts.bounce);
    if (!root->IsHit(ray, kEpsilon, kMaxT, hit)) {
      radiance += throughput * environment.Radiance(ray.GetDirection());
      break;
    }

    // Primitives return an unnormalised normal. Normalise it once and write
    // it back, so the BSDF reads from `hit` the same normal used here.
    // hit_point is the shadow-ray origin, held off the surface so those rays
    // do not self-hit.
    const glm::vec3 normal = normalize(hit.GetNormal());
    hit.SetNormal(normal);
    const glm::vec3 hit_point = OffsetFromSurface(hit.GetHitPoint(), normal);
    const glm::vec3 view_dir =
        -normalize(ray.GetDirection());  // AWAY from surface
    Material* material = hit.GetMaterial();

    // Crude next event estimation. A point light is a Dirac delta with
    // zero solid angle, so BSDF sampling can never draw a direction
    // that lands on one -- without this loop every scene is black.
    //
    // A delta BSDF is the mirror of that problem: it answers Eval with zero
    // for every direction but its one, and the light is never on it. Casting
    // the shadow ray anyway buys a full traversal and multiplies it by zero.
    if (!material->IsSpecular()) {
      for (Light* light : lights) {
        Ray shade_ray;
        shade_ray.SetOrigin(hit_point);
        shade_ray.SetDirection(light->position - hit_point);

        // Anything in the way: this light is occluded, skip it. The ray is
        // NOT normalized, so the light sits at t = 1 and the far bound has
        // to be 1 -- with kMaxT, geometry behind the light occludes it too.
        HitRecord occlusion;
        if (rt::stats::kEnabled) ++counts.shadow;
        if (root->IsHit(shade_ray, kEpsilon, 1.0f, occlusion)) continue;

        const glm::vec3 light_dir = normalize(shade_ray.GetDirection());
        radiance += throughput * material->Eval(view_dir, hit, light_dir) *
                    std::max(0.0f, dot(normal, light_dir)) * light->colour;
      }
    }

    float pdf;
    glm::vec3 brdf;
    const glm::vec3 out = material->Sample(rng, view_dir, hit, &pdf, &brdf);
    if (pdf <= 0.0f) break;  // scattered below the surface

    // For cosine-weighted Lambertian this reduces to throughput *= albedo.
    // A delta lobe skips the estimator: its brdf is already the weight,
    // and dividing then multiplying by the same cosine drifts in float.
    if (material->IsSpecular())
      throughput *= brdf;
    else
      throughput *= brdf * std::fabs(dot(out, normal)) / pdf;

    // The usual exit. Unbiased: a path survives with probability q and
    // its weight is divided by q, so the estimator is unchanged.
    if (bounces >= kRrStartDepth) {
      const float q = std::min(
          0.95f, std::max(throughput.x, std::max(throughput.y, throughput.z)));
      if (rng.Next() >= q) break;
      throughput /= q;
    }

    if (bounces + 1 >= max_depth) break;  // safety valve, biased

    // Reflection stays on normal's side; transmission crosses to the other
    // one, so the epsilon nudge has to follow `out`, not always +normal, or
    // a transmitted ray starts back inside the surface it just left.
    const glm::vec3 scatter_origin = OffsetFromSurface(
        hit.GetHitPoint(), dot(normal, out) >= 0.0f ? normal : -normal);
    ray.SetOrigin(scatter_origin);
    ray.SetDirection(out);
  }
  return radiance;
}
