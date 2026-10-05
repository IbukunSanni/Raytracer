// Raytracer -- percent-complete reporting for long renders

#ifndef RAYTRACER_SRC_RENDER_PROGRESS_H_
#define RAYTRACER_SRC_RENDER_PROGRESS_H_

#include <atomic>
#include <chrono>
#include <cstddef>

// A row of one sample is the unit of work, counted across every thread.
// Nothing prints until the render has run longer than a quiet period, so a
// render that finishes promptly stays silent.
class Progress {
 public:
  // Call before any thread reports a row.
  void Start(size_t rows_total);

  // Counts one finished row and logs a percentage every tenth of the
  // render. Touched once per row rather than once per pixel, so the
  // contention is far below the cost of tracing the row it counts.
  void RowDone();

 private:
  std::atomic<size_t> rows_done_{0};
  size_t rows_total_ = 0;
  size_t rows_per_report_ = 0;
  std::chrono::steady_clock::time_point start_;
};

#endif  // RAYTRACER_SRC_RENDER_PROGRESS_H_
