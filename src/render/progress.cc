// Raytracer -- percent-complete reporting for long renders

#include "render/progress.h"

#include <algorithm>

#include "core/log.h"

static const std::chrono::milliseconds kProgressQuietPeriod(1000);

void Progress::Start(size_t rows_total) {
  rows_done_.store(0, std::memory_order_relaxed);
  rows_total_ = rows_total;
  rows_per_report_ = std::max<size_t>(1, rows_total / 10);
  start_ = std::chrono::steady_clock::now();
}

void Progress::RowDone() {
  if (rows_per_report_ == 0) return;

  const size_t done = rows_done_.fetch_add(1, std::memory_order_relaxed) + 1;

  // Only the thread whose row lands exactly on a tenth reports it, and the
  // last tenth is left to the line that announces the finished render.
  if (done % rows_per_report_ != 0 || done >= rows_total_) return;

  if (std::chrono::steady_clock::now() - start_ < kProgressQuietPeriod) return;

  LOG_INFO(kRender) << (100 * done / rows_total_) << "%";
}
