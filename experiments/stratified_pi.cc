// Random versus stratified estimates of pi.

#include <cstdio>
#include <iomanip>
#include <iostream>

#include "render/sampling.h"

int main() {
  Rng rng(1u);
  std::cout << std::fixed << std::setprecision(12);

  int inside_circle = 0;
  int inside_circle_stratified = 0;
  int sqrt_N = 1000;

  for (int i = 0; i < sqrt_N; i++) {
    for (int j = 0; j < sqrt_N; j++) {
      auto x = rng.Range(-1, 1);
      auto y = rng.Range(-1, 1);
      if (x * x + y * y < 1) inside_circle++;

      x = 2 * ((i + rng.Next()) / sqrt_N) - 1;
      y = 2 * ((j + rng.Next()) / sqrt_N) - 1;
      if (x * x + y * y < 1) inside_circle_stratified++;
    }
  }

  std::cout << "Regular    Estimate of Pi = "
            << (4.0 * inside_circle) / (sqrt_N * sqrt_N) << '\n'
            << "Stratified Estimate of Pi = "
            << (4.0 * inside_circle_stratified) / (sqrt_N * sqrt_N) << '\n';
}
