
#include "render/renderer.h"

#include <lodepng/lodepng.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <glm/ext.hpp>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/log.h"
#include "core/ray.h"
#include "geometry/bvh.h"
#include "render/camera.h"
#include "render/framebuffer.h"
#include "render/sampling.h"
#include "scene/material.h"

// AA, depth of field and the bounce cap are runtime settings, not
// compile-time #defines. Each is a field of the gr.render table in Lua, so
// a scene that renders more than once can vary them per call.

static const float kMaxRgb = 255.0f;  // 8-bit channel max
static const float kMaxT =
    std::numeric_limits<float>::max();  // unbounded ray length
static const int kRrStartDepth = 3;     // bounces taken before roulette begins

// Set from the gr.render table before each render. Defaults: one sample,
// pinhole camera, and a bounce cap deep enough for ordinary scenes.
static LensConfig g_lens;
static int g_samples_per_pixel = 1;

// Safety valve for pathological geometry -- a hall of mirrors, or light
// trapped in a box. Russian roulette is the real termination and is
// unbiased; this cut is not, so it should almost never fire. Glass needs
// more of it than anything else: a hollow sphere is four crossings before
// the ray is even clear of it.
static int g_max_depth = 8;
static int g_snapshot_interval = 0;  // 0 == final image only
static std::string g_output_path;
static std::string g_background_path;  // empty => uniform `ambient` environment
static tonemap::Config g_tonemap;      // defaults: no tone map, sRGB on

// Progress reporting. A row of one sample is the unit of work, counted across
// every thread. Nothing prints until the render has run longer than the quiet
// period, so a render that finishes promptly stays silent.
static std::atomic<size_t> g_rows_done(0);
static size_t g_rows_total = 0;
static size_t g_rows_per_report = 0;
static std::chrono::steady_clock::time_point g_render_start;
static const std::chrono::milliseconds kProgressQuietPeriod(1000);

void SetLens(float aperture_radius, float focus_distance, int samples) {
  g_lens.aperture_radius = aperture_radius;
  g_lens.focus_distance = focus_distance;
  g_lens.samples = samples;
}

void SetMaxDepth(int bounces) { g_max_depth = (bounces < 1) ? 1 : bounces; }

void SetSamplesPerPixel(int samples) {
  g_samples_per_pixel = (samples < 1) ? 1 : samples;
}

void SetSnapshotInterval(int samples) {
  g_snapshot_interval = (samples < 0) ? 0 : samples;
}

void SetOutputPath(const std::string& path) { g_output_path = path; }

void SetBackground(const std::string& path) { g_background_path = path; }

void SetToneMap(const tonemap::Config& cfg) { g_tonemap = cfg; }

const tonemap::Config& GetToneMap() { return g_tonemap; }

// "renders/out.png" at 16 spp -> "renders/out_0016spp.png", so a
// convergence series doesn't overwrite itself.
static std::string SnapshotPath(const std::string& path, size_t samples) {
  std::string stem = path;
  std::string ext;
  const size_t dot = path.find_last_of('.');
  const size_t sep = path.find_last_of("/\\");
  if (dot != std::string::npos && (sep == std::string::npos || dot > sep)) {
    stem = path.substr(0, dot);
    ext = path.substr(dot);
  }

  std::ostringstream oss;
  oss << stem << "_" << std::setfill('0') << std::setw(4) << samples << "spp"
      << ext;
  return oss.str();
}

//---------------------------------------------------------------------
// Radiance arriving from outside the scene, for a ray that hit nothing.
//
// A function of direction alone, so the camera ray and every bounce ray
// see the same environment. The old lookup was screen-space, which is
// meaningless for a bounce ray -- it has no pixel -- and meant a sphere
// could never match the background the furnace test compares it to.
//
// With no texture loaded this returns `ambient`: a uniform emissive
// environment, which is exactly the furnace condition.
static glm::vec3 Environment(const glm::vec3& dir_vec, const glm::vec3& ambient,
                             const LoadedPng& bg_png) {
  const int tex_w = static_cast<int>(bg_png.loaded_width);
  const int tex_h = static_cast<int>(bg_png.loaded_height);
  if (tex_w <= 0 || tex_h <= 0) {
    return ambient;
  }

  // Lat-long (equirectangular): azimuth about +y to u, polar angle to v.
  const glm::vec3 d_vec = normalize(dir_vec);
  const float u = 0.5f + std::atan2(d_vec.x, -d_vec.z) / (2.0f * kPI);
  const float v = std::acos(glm::clamp(d_vec.y, -1.0f, 1.0f)) / kPI;

  const int tx = glm::clamp(static_cast<int>(u * tex_w), 0, tex_w - 1);
  const int ty = glm::clamp(static_cast<int>(v * tex_h), 0, tex_h - 1);

  const size_t idx =
      4u * (static_cast<size_t>(ty) * static_cast<size_t>(tex_w) +
            static_cast<size_t>(tx));

  // The PNG holds sRGB bytes; linearise them so they enter shading as
  // radiance. Image::SavePng re-encodes on the way out.
  return glm::vec3(static_cast<float>(tonemap::DecodeSrgb(
                       bg_png.rgba[idx] / static_cast<double>(kMaxRgb))),
                   static_cast<float>(tonemap::DecodeSrgb(
                       bg_png.rgba[idx + 1] / static_cast<double>(kMaxRgb))),
                   static_cast<float>(tonemap::DecodeSrgb(
                       bg_png.rgba[idx + 2] / static_cast<double>(kMaxRgb))));
}

// A hit point pushed clear of the surface along dir. The offset scales with
// the point's own magnitude because kEpsilon is absolute: past a few hundred
// units it is under one float ULP, and an unscaled nudge rounds away to zero.
static glm::vec3 OffsetFromSurface(const glm::vec3& hit_point,
                                   const glm::vec3& dir) {
  const float scale = std::max({std::fabs(hit_point.x), std::fabs(hit_point.y),
                                std::fabs(hit_point.z), 1.0f});
  return hit_point + dir * (kEpsilon * scale);
}

//---------------------------------------------------------------------
// Trace one path: bounce until it escapes, dies to roulette, or hits the
// depth cap, accumulating radiance weighted by the throughput carried so
// far. One loop iteration is one ray cast.
glm::vec3 RayTraceRgb(
    SceneNode* root,
    Ray ray,  // by value: the loop advances it
    Rng& rng,
    const glm::vec3& ambient,  // uniform environment radiance
    const std::list<Light*>& lights,
    const LoadedPng& bg_png  // environment texture, may be empty
) {
  glm::vec3 radiance(0.0f);
  glm::vec3 throughput(1.0f);

  for (int bounces = 0;; ++bounces) {
    // kEpsilon as t_min: ignore hits right at the ray origin.
    HitRecord record;
    if (!root->IsHit(ray, kEpsilon, kMaxT, record)) {
      radiance += throughput * Environment(ray.GetDirection(), ambient, bg_png);
      break;
    }

    // Read into locals: the record is geometry output, not scratch
    // space. normal is normalised here because primitives return an
    // unnormalised normal; hit_point is the shadow-ray origin, held off the
    // surface so those rays do not self-hit.
    const glm::vec3 normal = normalize(record.GetNormal());
    const glm::vec3 hit_point = OffsetFromSurface(record.GetHitPoint(), normal);
    const glm::vec3 view_dir =
        -normalize(ray.GetDirection());  // AWAY from surface
    Material* material = record.GetMaterial();

    // Crude next event estimation. A point light is a Dirac delta with
    // zero solid angle, so BSDF sampling can never draw a direction
    // that lands on one -- without this loop every scene is black.
    for (Light* light : lights) {
      Ray shade_ray;
      shade_ray.SetOrigin(hit_point);
      shade_ray.SetDirection(light->position - hit_point);

      // Anything in the way: this light is occluded, skip it.
      HitRecord occlusion;
      if (root->IsHit(shade_ray, kEpsilon, kMaxT, occlusion)) continue;

      const glm::vec3 light_dir = normalize(shade_ray.GetDirection());
      radiance += throughput * material->Eval(view_dir, normal, light_dir) *
                  std::max(0.0f, dot(normal, light_dir)) * light->colour;
    }

    float pdf;
    glm::vec3 brdf;
    const glm::vec3 out = material->Sample(rng, view_dir, normal, &pdf, &brdf);
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

    if (bounces + 1 >= g_max_depth)
      break;  // safety valve, biased -- see SetMaxDepth

    // Reflection stays on normal's side; transmission crosses to the other
    // one, so the epsilon nudge has to follow `out`, not always +normal, or
    // a transmitted ray starts back inside the surface it just left.
    const glm::vec3 scatter_origin = OffsetFromSurface(
        record.GetHitPoint(), dot(normal, out) >= 0.0f ? normal : -normal);
    ray.SetOrigin(scatter_origin);
    ray.SetDirection(out);
  }
  return radiance;
}
//---------------------------------------------------------------------

// Count one finished row and print a percentage every tenth of the render.
// The counter is touched once per row rather than once per pixel, so the
// contention is far below the cost of tracing the row it counts.
static void ReportRowDone() {
  if (g_rows_per_report == 0) return;

  const size_t done = g_rows_done.fetch_add(1, std::memory_order_relaxed) + 1;

  // Only the thread whose row lands exactly on a tenth reports it, and the
  // last tenth is left to the line that announces the finished render.
  if (done % g_rows_per_report != 0 || done >= g_rows_total) return;

  if (std::chrono::steady_clock::now() - g_render_start < kProgressQuietPeriod)
    return;

  LOG_INFO(kRender) << (100 * done / g_rows_total) << "%";
}

// Trace `samples_to_add` jittered samples per pixel for rows
// [start_row, end_row) and accumulate them into the shared framebuffer.
// Bands own disjoint rows, so no locking. Jittering every Sample (no
// unjittered centre sample) is what anti-aliases the edges.
void RenderBand(
    Framebuffer& accum, size_t start_row, size_t end_row,
    size_t samples_to_add,  // samples per pixel this call traces
    size_t samples_done,    // already accumulated; picks a fresh RNG stream
    glm::vec3 corner_dir_vec, size_t h, size_t w, const CameraBasis& cam,
    const glm::vec3& ambient, const std::list<Light*>& lights, SceneNode* root,
    const LoadedPng& bg_png, int thread_idx) {
  // Per-thread RNG, seeded from thread index and samples_done so chunks
  // don't replay jitter. Deterministic: same scene + thread count => same
  // image.
  Rng rng(
      static_cast<uint32_t>(1u + thread_idx * 9781u + samples_done * 7919u));

  const glm::vec3& eye = cam.eye;
  const glm::vec3& u_vec = cam.u_vec;
  const glm::vec3& v_vec = cam.v_vec;

  for (size_t sample = 0; sample < samples_to_add; ++sample) {
    for (size_t y = start_row; y < end_row; ++y) {
      for (size_t x = 0; x < w; ++x) {
        // Direction through the pixel centre. The half-pixel is what makes
        // it the centre and not the top-left corner of the pixel.
        const glm::vec3 centre_dir_vec =
            corner_dir_vec + (static_cast<float>(x) + 0.5f) * u_vec -
            (static_cast<float>(y) + 0.5f) * v_vec;

        // Jitter within the pixel footprint: +/- half a pixel each way. The
        // interval is symmetric, so the sign in front of v does not matter.
        // TODO: stratify the offsets for faster convergence.
        const glm::vec3 dir_vec = centre_dir_vec + (rng.Next() - 0.5f) * u_vec +
                                  (rng.Next() - 0.5f) * v_vec;

        // Pinhole ray, or a lens ray for depth of field.
        Ray ray;
        if (g_lens.Enabled()) {
          ray = ThinLensRay(cam, dir_vec, g_lens, rng);
        } else {
          ray.SetOrigin(eye);
          ray.SetDirection(dir_vec);
        }

        const glm::vec3 radiance =
            RayTraceRgb(root, ray, rng, ambient, lights, bg_png);

        accum.Add(x, y, radiance);
      }

      ReportRowDone();
    }
  }
}
//---------------------------------------------------------------------
void Render(SceneNode* root,  // scene graph
            Image& image,     // output, already sized w x h

            const glm::vec3& eye,   // camera position
            const glm::vec3& view,  // look direction (not a target point)
            const glm::vec3& up,
            double fovy,  // vertical field of view, degrees

            const glm::vec3& ambient, const std::list<Light*>& lights) {
  auto start_time = std::chrono::high_resolution_clock::now();

  // The scene header is one statement per line, at debug. At 13 lines a
  // frame it would otherwise dominate an 85-frame animation log.
  if (rt::log::Enabled(rt::log::Level::kDebug, rt::log::Cat::kRender)) {
    LOG_DEBUG(kRender) << "render " << image.Width() << "x" << image.Height();
    LOG_DEBUG(kRender) << "  root    " << *root;
    LOG_DEBUG(kRender) << "  eye     " << glm::to_string(eye);
    LOG_DEBUG(kRender) << "  view    " << glm::to_string(view);
    LOG_DEBUG(kRender) << "  up      " << glm::to_string(up);
    LOG_DEBUG(kRender) << "  fovy    " << fovy;
    LOG_DEBUG(kRender) << "  ambient " << glm::to_string(ambient);
    for (const Light* light : lights) {
      LOG_DEBUG(kRender) << "  light   " << *light;
    }
  }

  size_t h = image.Height();
  size_t w = image.Width();

  // Environment texture, from gr.set_background. Left empty the scene
  // gets a uniform environment of radiance `ambient` instead.
  LoadedPng bg_png;
  bg_png.loaded_width = 0;
  bg_png.loaded_height = 0;
  if (!g_background_path.empty()) {
    const unsigned error =
        lodepng::decode(bg_png.rgba, bg_png.loaded_width, bg_png.loaded_height,
                        g_background_path);
    if (error) {
      // Fall back to the uniform environment rather than rendering
      // against whatever half-decoded bytes are in the buffer.
      bg_png.loaded_width = 0;
      bg_png.loaded_height = 0;
      LOG_ERROR(kRender) << "environment texture: "
                         << lodepng_error_text(error);
    } else {
      LOG_DEBUG(kRender) << "environment texture " << bg_png.loaded_width << "x"
                         << bg_png.loaded_height;
    }
  } else {
    LOG_DEBUG(kRender) << "uniform environment " << glm::to_string(ambient);
  }

  // Camera basis: u = screen right, v = screen up, w = forward. The triple
  // is left-handed (u x v = -w); that is the price of w being the view axis
  // rather than pointing back out of the screen.
  const glm::vec3 w_vec = normalize(view);
  const glm::vec3 u_vec = normalize(cross(w_vec, up));
  const glm::vec3 v_vec = cross(u_vec, w_vec);

  // Distance to the image plane that makes it fovy tall. Measured in pixels,
  // because one unit of u or v is one pixel, so the film is exactly w x h.
  const float d_float = static_cast<float>(static_cast<double>(h) / 2.0 /
                                           glm::tan(glm::radians(fovy / 2.0)));

  // Direction from the eye to the top-left corner of the film. RenderBand
  // walks right along +u and down along -v from here.
  const glm::vec3 corner_dir_vec = w_vec * d_float -
                                   u_vec * static_cast<float>(w) / 2.0f +
                                   v_vec * static_cast<float>(h) / 2.0f;

  // Pack the basis for the thin-lens code (aperture in u/v, focus along w).
  CameraBasis cam;
  cam.eye = eye;
  cam.u_vec = u_vec;
  cam.v_vec = v_vec;
  cam.w_vec = w_vec;

  if (g_lens.Enabled()) {
    LOG_INFO(kRender) << "lens: aperture " << g_lens.aperture_radius
                      << ", focus " << g_lens.focus_distance << ", "
                      << g_lens.samples << " samples/pixel";
  }
  BVH::ResetStats();

  // Progressive accumulation: samples add into a shared buffer that can
  // be resolved to an image at any time, so a snapshot is just a divide
  // and a PNG write, not a re-render.
  Framebuffer accum(w, h);

  // AA samples x lens samples.
  const size_t total_samples =
      static_cast<size_t>(g_samples_per_pixel) *
      static_cast<size_t>(g_lens.Enabled() ? g_lens.samples : 1);

  // One thread per hardware core, respawned per snapshot chunk (a single
  // spawn when snapshots are off).
  // TODO: replace the static bands with a tile queue.
  const unsigned int hw = std::thread::hardware_concurrency();
  const int num_threads = static_cast<int>((hw == 0) ? 16u : hw);

  {
    // One Line object so the whole sentence is a single log record,
    // rather than two that another thread could split.
    rt::log::Line ln(rt::log::Level::kInfo, rt::log::Cat::kRender);
    ln.Stream() << "rendering " << total_samples << " Sample(s)/pixel on "
                << num_threads << " threads";
    if (g_snapshot_interval > 0) {
      ln.Stream() << ", snapshot every " << g_snapshot_interval;
    }
  }

  std::vector<std::thread> threads(static_cast<size_t>(num_threads));

  g_rows_done.store(0, std::memory_order_relaxed);
  g_rows_total = total_samples * h;
  g_rows_per_report = std::max<size_t>(1, g_rows_total / 10);
  g_render_start = std::chrono::steady_clock::now();

  // Each iteration renders chunk_samples more, then optionally snapshots.
  size_t samples_done = 0;
  while (samples_done < total_samples) {
    const size_t remaining = total_samples - samples_done;
    const size_t chunk_samples =
        (g_snapshot_interval > 0)
            ? std::min(static_cast<size_t>(g_snapshot_interval),
                       remaining)  // up to the interval
            : remaining;           // everything left

    // Split rows across threads; spread the remainder one per thread.
    const size_t delta_h = h / static_cast<size_t>(num_threads);
    const size_t extra_h = h % static_cast<size_t>(num_threads);

    size_t start_row = 0;
    for (int i = 0; i < num_threads; i++) {
      const size_t end_row =
          start_row + delta_h + (static_cast<size_t>(i) < extra_h ? 1u : 0u);

      threads[static_cast<size_t>(i)] = std::thread(
          RenderBand, std::ref(accum), start_row, end_row, chunk_samples,
          samples_done, corner_dir_vec, h, w, std::cref(cam), ambient, lights,
          root, std::cref(bg_png), i);

      start_row = end_row;
    }

    for (int i = 0; i < num_threads; i++) {
      threads[static_cast<size_t>(i)].join();
    }

    accum.AddSamples(chunk_samples);
    samples_done += chunk_samples;

    // Marks a written snapshot. Without one the render is a single pass
    // and this would only ever restate the closing line.
    if (g_snapshot_interval > 0) {
      LOG_INFO(kRender) << samples_done << "/" << total_samples << " spp";
    }

    // Intermediate image; accumulation carries on untouched.
    if (g_snapshot_interval > 0 && samples_done < total_samples &&
        !g_output_path.empty()) {
      accum.Resolve(image);
      const std::string snap = SnapshotPath(g_output_path, samples_done);
      if (!image.SavePng(snap, g_tonemap)) {
        LOG_ERROR(kRender) << "snapshot write failed: " << snap;
      }
    }
  }

  accum.Resolve(image);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  LOG_INFO(kRender) << "done in " << duration.count() << " ms, "
                    << accum.SampleCount() << " spp";
  BVH::ReportStats("frame totals");
}
