// Area under exp(-x / 2pi) sin^2(x) over [0, 2pi], and the x that splits
// that area in half, both estimated from uniform samples.

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

#include "render/sampling.h"

// One draw: where it landed, and the curve's height there.
struct Sample {
  double x;
  double p_x;
};

int main() {
  constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
  constexpr unsigned int N = 10000;
  Rng rng(1u);
  std::vector<Sample> samples(N);
  double sum = 0.0;

  // Sample the curve uniformly over its domain, keeping every draw.
  for (unsigned int i = 0; i < N; i++) {
    const double x = rng.Range(0.0f, static_cast<float>(kTwoPi));
    const double sin_x = std::sin(x);
    const double p_x = std::exp(-x / kTwoPi) * sin_x * sin_x;
    sum += p_x;
    samples[i] = {x, p_x};
  }

  // In x order, a running sum of heights is the area to the left of x.
  std::sort(samples.begin(), samples.end(),
            [](const Sample& a, const Sample& b) { return a.x < b.x; });

  // Walk up x until the running area reaches half the total.
  const double half_sum = sum / 2.0;
  double halfway_point = 0.0;
  double accum = 0.0;
  for (unsigned int i = 0; i < N; i++) {
    accum += samples[i].p_x;
    if (accum >= half_sum) {
      halfway_point = samples[i].x;
      break;
    }
  }

  std::cout << std::fixed << std::setprecision(12);
  // Mean height times the domain's width estimates the area.
  std::cout << "Average = " << sum / N << '\n';
  std::cout << "Area under curve = " << kTwoPi * sum / N << '\n';
  std::cout << "Halfway = " << halfway_point << '\n';
}
