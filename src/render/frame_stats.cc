// Raytracer -- per-frame ray counts and the end-of-frame report

#include "render/frame_stats.h"

#include <atomic>
#include <iomanip>
#include <sstream>

#include "core/log.h"
#include "core/stats.h"
#include "geometry/bvh.h"

static std::atomic<long long> g_rays_primary(0);
static std::atomic<long long> g_rays_shadow(0);
static std::atomic<long long> g_rays_bounce(0);

void ResetRayTotals() {
  g_rays_primary.store(0);
  g_rays_shadow.store(0);
  g_rays_bounce.store(0);
}

void AddToRayTotals(const RayCounts& counts) {
  if (!rt::stats::kEnabled) return;
  g_rays_primary.fetch_add(counts.primary, std::memory_order_relaxed);
  g_rays_shadow.fetch_add(counts.shadow, std::memory_order_relaxed);
  g_rays_bounce.fetch_add(counts.bounce, std::memory_order_relaxed);
}

RayCounts RayTotals() {
  RayCounts totals;
  totals.primary = g_rays_primary.load();
  totals.shadow = g_rays_shadow.load();
  totals.bounce = g_rays_bounce.load();
  return totals;
}

void LogFrameReport(const FrameSummary& summary) {
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(summary.elapsed);
  LOG_INFO(kRender) << "done in " << ms.count() << " ms, " << summary.spp
                    << " spp";

  // No rays/sec here: a counting run is slower than a timed one, so the
  // rate takes its rays from this line and its time from a separate run.
  const RayCounts rays = RayTotals();
  if (rt::stats::kEnabled) {
    LOG_DEBUG(kRender) << "rays: primary " << rays.primary << ", shadow "
                       << rays.shadow << ", bounce " << rays.bounce
                       << ", total "
                       << rays.primary + rays.shadow + rays.bounce;
  } else {
    LOG_DEBUG(kRender) << "rays: counts off (set RT_STATS=1)";
  }
  BVH::ReportStats("frame totals");

  // Counts appear only with RT_STATS=1, and such a run is slower, so the
  // bench script takes render_ms from runs without it.
  const double render_ms =
      std::chrono::duration<double, std::milli>(summary.elapsed).count();
  std::ostringstream bench;
  bench << "bench width=" << summary.width << " height=" << summary.height
        << " spp=" << summary.spp << " threads=" << summary.threads
        << " traversal=" << BVH::TraversalName(BVH::Traversal())
        << " split=" << BVH::SplitName(BVH::Split())
        << " leaf=" << BVH::kLeafSize << std::fixed << std::setprecision(3)
        << " render_ms=" << render_ms
        << " stats=" << (rt::stats::kEnabled ? "on" : "off");
  if (rt::stats::kEnabled) {
    const BVH::FrameStats totals = BVH::Totals();
    bench << " build_ms=" << totals.build_ms << " rays_primary=" << rays.primary
          << " rays_shadow=" << rays.shadow << " rays_bounce=" << rays.bounce
          << " calls=" << totals.calls << " nodes=" << totals.nodes_visited
          << " triangles=" << totals.triangles_tested;
  }
  LOG_DEBUG(kRender) << bench.str();
}
