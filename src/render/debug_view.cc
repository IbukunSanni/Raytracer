#include "render/debug_view.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#include "core/hit_record.h"
#include "core/log.h"
#include "core/stats.h"
#include "render/sampling.h"

namespace {

RenderView ParseView() {
  const char* value = std::getenv("RT_VIEW");
  if (value == nullptr || *value == '\0') return RenderView::kShaded;
  for (RenderView view : {RenderView::kShaded, RenderView::kNormal}) {
    if (std::strcmp(value, ViewName(view)) == 0) return view;
  }
  LOG_ERROR(kRender) << "unknown RT_VIEW '" << value
                     << "'; expected shaded or normal";
  std::exit(EXIT_FAILURE);
}

}  // namespace

RenderView ActiveView() {
  static const RenderView kView = ParseView();
  return kView;
}

const char* ViewName(RenderView view) {
  switch (view) {
    case RenderView::kShaded:
      return "shaded";
    case RenderView::kNormal:
      return "normal";
  }
  return "?";
}

glm::vec3 TraceView(SceneNode* root, Ray ray, RenderView view,
                    RayCounts& counts) {
  if (rt::stats::kEnabled) ++counts.primary;
  HitRecord record;
  if (!root->IsHit(ray, kEpsilon, std::numeric_limits<float>::max(), record)) {
    return glm::vec3(0.0f);
  }

  switch (view) {
    case RenderView::kNormal:
      return 0.5f * (glm::normalize(record.GetNormal()) + 1.0f);
    case RenderView::kShaded:
      break;  // not a debug view; RenderBand calls RayTraceRgb instead
  }
  return glm::vec3(0.0f);
}
