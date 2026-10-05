// Raytracer -- render settings, and the loop that turns a scene into pixels
//
// Render sets up one frame and splits its rows into bands, one per thread.
// Each band traces its pixels with RayTraceRgb.

#include "render/renderer.h"

#include <algorithm>
#include <chrono>
#include <glm/ext.hpp>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/log.h"
#include "core/ray.h"
#include "geometry/bvh.h"
#include "render/camera.h"
#include "render/environment.h"
#include "render/frame_stats.h"
#include "render/framebuffer.h"
#include "render/integrator.h"
#include "render/progress.h"
#include "render/sampling.h"

namespace {

// AA, depth of field and the bounce cap are runtime settings, not
// compile-time #defines. Each is a field of the gr.render table in Lua, so
// a scene that renders more than once can vary them per call.
struct RenderSettings {
  LensConfig lens;  // pinhole unless the scene asks for a lens
  int samples_per_pixel = 1;
  // The path tracer's bounce cap. Glass needs more of it than anything
  // else: a hollow sphere is four crossings before the ray is even clear.
  int max_depth = 8;
  int snapshot_interval = 0;    // 0 == final image only
  std::string output_path;      // where snapshots are written beside
  std::string background_path;  // empty => uniform `ambient` environment
  // Decoded once, in SetBackground, outside the render timer. Null when the
  // path is empty or failed to decode.
  std::shared_ptr<const EnvironmentTexture> background;
  tonemap::Config tonemap;  // defaults: no tone map, sRGB on
};

RenderSettings g_settings;

// Everything a band reads for one frame. Only accum and progress are
// written, and bands own disjoint rows of accum, so nothing needs a lock.
struct Frame {
  SceneNode* root;
  const std::list<Light*>& lights;
  const Environment& environment;
  const CameraBasis& cam;
  glm::vec3 corner_dir_vec;  // eye to the film's top-left corner
  size_t width;
  size_t height;
  Framebuffer& accum;
  Progress& progress;
};

// "renders/out.png" at 16 spp -> "renders/out_0016spp.png", so a
// convergence series doesn't overwrite itself.
std::string SnapshotPath(const std::string& path, size_t samples) {
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

// The scene header is one statement per line, at debug. At 13 lines a
// frame it would otherwise dominate an 85-frame animation log.
void LogSceneHeader(SceneNode* root, const Image& image, const glm::vec3& eye,
                    const glm::vec3& view, const glm::vec3& up, double fovy,
                    const glm::vec3& ambient, const std::list<Light*>& lights) {
  if (!rt::log::Enabled(rt::log::Level::kDebug, rt::log::Cat::kRender)) return;
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

// Trace `samples_to_add` jittered samples per pixel for rows
// [start_row, end_row) and accumulate them into the shared framebuffer.
// Jittering every sample (no unjittered centre sample) is what anti-aliases
// the edges.
void RenderBand(const Frame& frame, size_t start_row, size_t end_row,
                size_t samples_to_add,  // samples per pixel this call traces
                size_t samples_done,    // already accumulated; picks the stream
                int thread_idx) {
  // Per-thread RNG, seeded from thread index and samples_done so chunks
  // don't replay jitter. Deterministic: same scene + thread count => same
  // image.
  Rng rng(
      static_cast<uint32_t>(1u + thread_idx * 9781u + samples_done * 7919u));
  RayCounts counts;

  const LensConfig& lens = g_settings.lens;
  const glm::vec3& u_vec = frame.cam.u_vec;
  const glm::vec3& v_vec = frame.cam.v_vec;

  for (size_t sample = 0; sample < samples_to_add; ++sample) {
    for (size_t y = start_row; y < end_row; ++y) {
      for (size_t x = 0; x < frame.width; ++x) {
        // Direction through the pixel centre. The half-pixel is what makes
        // it the centre and not the top-left corner of the pixel.
        const glm::vec3 centre_dir_vec =
            frame.corner_dir_vec + (static_cast<float>(x) + 0.5f) * u_vec -
            (static_cast<float>(y) + 0.5f) * v_vec;

        // Jitter within the pixel footprint: +/- half a pixel each way. The
        // interval is symmetric, so the sign in front of v does not matter.
        // TODO: stratify the offsets for faster convergence.
        const glm::vec3 dir_vec = centre_dir_vec + (rng.Next() - 0.5f) * u_vec +
                                  (rng.Next() - 0.5f) * v_vec;

        // Pinhole ray, or a lens ray for depth of field.
        Ray ray;
        if (lens.Enabled()) {
          ray = ThinLensRay(frame.cam, dir_vec, lens, rng);
        } else {
          ray.SetOrigin(frame.cam.eye);
          ray.SetDirection(dir_vec);
        }

        const glm::vec3 radiance =
            RayTraceRgb(frame.root, ray, rng, frame.environment, frame.lights,
                        g_settings.max_depth, counts);

        frame.accum.Add(x, y, radiance);
      }

      frame.progress.RowDone();
    }
  }

  BVH::FlushThreadStats();
  AddToRayTotals(counts);
}

// Renders samples_to_add more samples per pixel on num_threads threads,
// one band of rows each, and returns when every band has finished.
void RenderChunk(const Frame& frame, int num_threads, size_t samples_to_add,
                 size_t samples_done) {
  std::vector<std::thread> threads(static_cast<size_t>(num_threads));

  // Split rows across threads; spread the remainder one per thread.
  const size_t delta_h = frame.height / static_cast<size_t>(num_threads);
  const size_t extra_h = frame.height % static_cast<size_t>(num_threads);

  size_t start_row = 0;
  for (int i = 0; i < num_threads; i++) {
    const size_t end_row =
        start_row + delta_h + (static_cast<size_t>(i) < extra_h ? 1u : 0u);

    threads[static_cast<size_t>(i)] =
        std::thread(RenderBand, std::cref(frame), start_row, end_row,
                    samples_to_add, samples_done, i);

    start_row = end_row;
  }

  for (std::thread& thread : threads) {
    thread.join();
  }
}

}  // namespace

void SetLens(float aperture_radius, float focus_distance, int samples,
             ApertureShape shape) {
  g_settings.lens.aperture_radius = aperture_radius;
  g_settings.lens.focus_distance = focus_distance;
  g_settings.lens.samples = samples;
  g_settings.lens.shape = shape;
}

void SetMaxDepth(int bounces) {
  g_settings.max_depth = (bounces < 1) ? 1 : bounces;
}

void SetSamplesPerPixel(int samples) {
  g_settings.samples_per_pixel = (samples < 1) ? 1 : samples;
}

void SetSnapshotInterval(int samples) {
  g_settings.snapshot_interval = (samples < 0) ? 0 : samples;
}

void SetOutputPath(const std::string& path) { g_settings.output_path = path; }

// Setting the path already decoded keeps that texture: an animation sets it
// once and renders every frame against it.
void SetBackground(const std::string& path) {
  if (path == g_settings.background_path && g_settings.background) return;
  g_settings.background_path = path;
  g_settings.background = path.empty() ? nullptr : LoadEnvironmentTexture(path);
}

void SetToneMap(const tonemap::Config& cfg) { g_settings.tonemap = cfg; }

const tonemap::Config& GetToneMap() { return g_settings.tonemap; }

void Render(SceneNode* root,  // scene graph
            Image& image,     // output, already sized w x h

            const glm::vec3& eye,   // camera position
            const glm::vec3& view,  // look direction (not a target point)
            const glm::vec3& up,
            double fovy,  // vertical field of view, degrees

            const glm::vec3& ambient, const std::list<Light*>& lights) {
  const auto start_time = std::chrono::steady_clock::now();
  const RenderSettings& settings = g_settings;

  LogSceneHeader(root, image, eye, view, up, fovy, ambient, lights);

  const size_t h = image.Height();
  const size_t w = image.Width();

  // Environment texture, from gr.set_background. Left empty the scene
  // gets a uniform environment of radiance `ambient` instead.
  const Environment environment(settings.background.get(), ambient);

  const CameraBasis cam = MakeCameraBasis(eye, view, up);
  const glm::vec3 corner_dir_vec = FilmCornerDirection(cam, w, h, fovy);

  if (settings.lens.Enabled()) {
    LOG_INFO(kRender) << "lens: aperture " << settings.lens.aperture_radius
                      << ", focus " << settings.lens.focus_distance << ", "
                      << settings.lens.samples << " samples/pixel";
  }
  BVH::ResetStats();
  ResetRayTotals();

  // Progressive accumulation: samples add into a shared buffer that can
  // be resolved to an image at any time, so a snapshot is just a divide
  // and a PNG write, not a re-render.
  Framebuffer accum(w, h);

  // AA samples x lens samples.
  const size_t total_samples =
      static_cast<size_t>(settings.samples_per_pixel) *
      static_cast<size_t>(settings.lens.Enabled() ? settings.lens.samples : 1);

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
    if (settings.snapshot_interval > 0) {
      ln.Stream() << ", snapshot every " << settings.snapshot_interval;
    }
  }

  Progress progress;
  progress.Start(total_samples * h);
  const Frame frame{root, lights, environment, cam,     corner_dir_vec,
                    w,    h,      accum,       progress};

  // Each iteration renders chunk_samples more, then optionally snapshots.
  size_t samples_done = 0;
  while (samples_done < total_samples) {
    const size_t remaining = total_samples - samples_done;
    const size_t chunk_samples =
        (settings.snapshot_interval > 0)
            ? std::min(static_cast<size_t>(settings.snapshot_interval),
                       remaining)  // up to the interval
            : remaining;           // everything left

    RenderChunk(frame, num_threads, chunk_samples, samples_done);

    accum.AddSamples(chunk_samples);
    samples_done += chunk_samples;

    // Marks a written snapshot. Without one the render is a single pass
    // and this would only ever restate the closing line.
    if (settings.snapshot_interval > 0) {
      LOG_INFO(kRender) << samples_done << "/" << total_samples << " spp";
    }

    // Intermediate image; accumulation carries on untouched.
    if (settings.snapshot_interval > 0 && samples_done < total_samples &&
        !settings.output_path.empty()) {
      accum.Resolve(image);
      const std::string snap = SnapshotPath(settings.output_path, samples_done);
      if (!image.SavePng(snap, settings.tonemap)) {
        LOG_ERROR(kRender) << "snapshot write failed: " << snap;
      }
    }
  }

  accum.Resolve(image);

  FrameSummary summary;
  summary.width = w;
  summary.height = h;
  summary.spp = accum.SampleCount();
  summary.threads = num_threads;
  summary.elapsed = std::chrono::steady_clock::now() - start_time;
  LogFrameReport(summary);
}
