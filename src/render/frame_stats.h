// Raytracer -- per-frame ray counts and the end-of-frame report

#ifndef RAYTRACER_SRC_RENDER_FRAME_STATS_H_
#define RAYTRACER_SRC_RENDER_FRAME_STATS_H_

#include <chrono>
#include <cstddef>

// Rays cast by one band, by kind. Counted in the band's own struct and added
// to the frame totals once when it ends, so nothing is shared per ray. Only
// counted when RT_STATS=1.
struct RayCounts {
  long long primary = 0;  // camera rays, one per pixel sample
  long long shadow = 0;   // one per light at each non-specular hit
  long long bounce = 0;   // scattered rays after the first hit
};

// Frame totals across every band. All zero unless RT_STATS=1.
void ResetRayTotals();
void AddToRayTotals(const RayCounts& counts);
RayCounts RayTotals();

// What the end-of-frame report needs from the render loop.
struct FrameSummary {
  size_t width = 0;
  size_t height = 0;
  size_t spp = 0;
  int threads = 0;
  std::chrono::steady_clock::duration elapsed{};
};

// Logs the closing "done in" line, the ray and BVH totals, and one
// machine-readable `bench` record for scripts/bench_bvh.sh.
void LogFrameReport(const FrameSummary& summary);

#endif  // RAYTRACER_SRC_RENDER_FRAME_STATS_H_
