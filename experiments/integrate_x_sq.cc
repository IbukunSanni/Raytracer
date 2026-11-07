// Importance-sampled estimates of the integral of x^2 + x/3 over [0, 6],
// exactly 78. Each pdf draws x through its inverse CDF and averages
// f(x)/pdf(x). All are unbiased; they differ only in variance.

#include <cmath>
#include <iomanip>
#include <iostream>

#include "render/sampling.h"

double F(double x) { return x * x + x / 3.0; }

// pdf(x) = 1/6, CDF x/6
double UniformIcd(double d) { return 6.0 * d; }
double UniformPdf(double) { return 1.0 / 6.0; }

// pdf(x) = x/18, CDF x^2/36
double LinearIcd(double d) { return 6.0 * std::sqrt(d); }
double LinearPdf(double x) { return x / 18.0; }

// pdf(x) = x^2/72, CDF x^3/216
double QuadraticIcd(double d) { return 6.0 * std::cbrt(d); }
double QuadraticPdf(double x) { return x * x / 72.0; }

// Prints the mean of f/pdf, its error against the exact value, the
// per-sample variance, and the standard error sqrt(variance / N) of the mean.
void Estimate(const char* name, double (*icd)(double), double (*pdf)(double),
              int N) {
  const double exact = 78.0;
  Rng rng(1u);
  double sum = 0.0;
  double sum_sq = 0.0;
  int n = 0;

  for (int i = 0; i < N; i++) {
    double z = rng.Next();
    if (z == 0.0) continue;  // x = 0 zeroes the linear and quadratic pdfs

    double x = icd(z);
    double w = F(x) / pdf(x);
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

  Estimate("uniform pdf 1/6", UniformIcd, UniformPdf, N);
  Estimate("linear pdf x/18", LinearIcd, LinearPdf, N);
  Estimate("quadratic pdf x^2/72", QuadraticIcd, QuadraticPdf, N);
}
