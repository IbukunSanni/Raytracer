// Importance-sampled estimates of the integral of x^2 over [0, 2], exactly 8/3.
// Each pdf draws x through its inverse CDF and averages f(x)/pdf(x). Both are
// unbiased; they differ only in how spread out the f/pdf values are.

#include <cmath>
#include <iomanip>
#include <iostream>

#include "render/sampling.h"

// pdf(x) = 1/2, CDF x/2
double UniformIcd(double d) { return 2.0 * d; }
double UniformPdf(double) { return 0.5; }

// pdf(x) = x/2, CDF x^2/4
double LinearIcd(double d) { return std::sqrt(4.0 * d); }
double LinearPdf(double x) { return x / 2.0; }

// Prints the mean of f/pdf, its error against 8/3, the per-sample variance,
// and the standard error sqrt(variance / N) of the mean.
void Estimate(const char* name, double (*icd)(double), double (*pdf)(double),
              int N) {
  const double exact = 8.0 / 3.0;
  Rng rng(1u);
  double sum = 0.0;
  double sum_sq = 0.0;
  int n = 0;

  for (int i = 0; i < N; i++) {
    double z = rng.Next();
    if (z == 0.0) continue;  // x = 0 gives a zero linear pdf and a NaN

    double x = icd(z);
    double w = x * x / pdf(x);
    sum += w;
    sum_sq += w * w;
    n++;
  }

  double mean = sum / n;
  double variance = sum_sq / n - mean * mean;
  double std_err = std::sqrt(variance / n);

  std::cout << name << '\n'
            << "  I         = " << mean << '\n'
            << "  error     = " << mean - exact << '\n'
            << "  variance  = " << variance << '\n'
            << "  std error = " << std_err << '\n';
}

int main() {
  std::cout << std::fixed << std::setprecision(12);
  const int N = 1000000;

  Estimate("uniform pdf 1/2", UniformIcd, UniformPdf, N);
  Estimate("linear pdf x/2", LinearIcd, LinearPdf, N);
}
