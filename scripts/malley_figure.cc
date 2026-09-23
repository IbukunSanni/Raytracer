// The numbers and the figure for posts/malleys-method.md, from the renderer's
// own sampler. Rerunning this regenerates both exactly (same compiler).
//
//   g++ -O2 -std=c++17 -I src -I third_party scripts/malley_figure.cc
//       third_party/lodepng/lodepng.cpp -o build/malley_figure
//   ./build/malley_figure docs/images/malley-hexagon.png
//
// (One command, wrapped here -- a trailing backslash in a // comment would
// continue the comment and warn under -Wall.)
//
// Left panel: the unit disk (blue) and the hexagon a bladed iris would put
// inside it (orange). Right panel: the distribution of cos(theta) after the
// lift. The grey line is the exact cosine-lobe density, p(cos) = 2 cos, which
// the disk follows and the hexagon does not.

#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>

#include "lodepng/lodepng.h"
#include "render/sampling.h"

namespace {

constexpr long kSamples = 2000000;
constexpr float kApothem = 0.8660254f;  // sqrt(3)/2, circumradius 1

// Regular hexagon inscribed in the unit circle, flat top and bottom.
bool InHexagon(float x, float y) {
  return std::fabs(y) <= kApothem &&
         std::fabs(kApothem * x + 0.5f * y) <= kApothem &&
         std::fabs(kApothem * x - 0.5f * y) <= kApothem;
}

glm::vec2 SampleUnitHexagon(Rng& rng) {
  for (int i = 0; i < 64; ++i) {
    const float x = rng.Range(-1.0f, 1.0f);
    const float y = rng.Range(-1.0f, 1.0f);
    if (InHexagon(x, y)) return glm::vec2(x, y);
  }
  return glm::vec2(0.0f, 0.0f);
}

float Lift(const glm::vec2& d) {
  return std::sqrt(std::fmax(0.0f, 1.0f - d.x * d.x - d.y * d.y));
}

// Moments of cos(theta), plus a histogram of it for the figure.
struct Lobe {
  double sum = 0.0, sum_sq = 0.0;
  std::vector<long> bins = std::vector<long>(20, 0);
  void Add(float c) {
    sum += c;
    sum_sq += static_cast<double>(c) * c;
    int b = static_cast<int>(c * 20.0f);
    if (b > 19) b = 19;
    if (b >= 0) ++bins[b];
  }
  double Mean() const { return sum / kSamples; }
  double MeanSq() const { return sum_sq / kSamples; }
  // Histogram height as a density, comparable to p(cos) = 2 cos.
  double Density(int b) const { return bins[b] * 20.0 / kSamples; }
};

//---------------------------------------------------------------------
// Just enough raster drawing for two panels. Distance-based, so edges come
// out antialiased without a library.
struct Canvas {
  int w, h;
  std::vector<unsigned char> px;
  Canvas(int width, int height) : w(width), h(height), px(width * height * 4) {
    for (size_t i = 0; i < px.size(); i += 4) {
      px[i] = px[i + 1] = px[i + 2] = 250;
      px[i + 3] = 255;
    }
  }
  void Blend(int x, int y, const glm::vec3& c, float a) {
    if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.0f) return;
    unsigned char* p = &px[(y * w + x) * 4];
    for (int k = 0; k < 3; ++k) {
      p[k] = static_cast<unsigned char>(p[k] * (1.0f - a) + c[k] * a + 0.5f);
    }
  }
  void Segment(glm::vec2 a, glm::vec2 b, float width, const glm::vec3& c) {
    const int x0 = static_cast<int>(std::fmin(a.x, b.x) - width) - 1;
    const int x1 = static_cast<int>(std::fmax(a.x, b.x) + width) + 1;
    const int y0 = static_cast<int>(std::fmin(a.y, b.y) - width) - 1;
    const int y1 = static_cast<int>(std::fmax(a.y, b.y) + width) + 1;
    const glm::vec2 ab = b - a;
    const float len_sq = std::fmax(glm::dot(ab, ab), 1e-6f);
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const glm::vec2 p(x + 0.5f, y + 0.5f);
        const float t =
            std::fmin(1.0f, std::fmax(0.0f, glm::dot(p - a, ab) / len_sq));
        const float d = glm::length(p - (a + t * ab));
        Blend(x, y, c, std::fmin(1.0f, std::fmax(0.0f, width * 0.5f - d + 0.5f)));
      }
    }
  }
};

const glm::vec3 kBlue(37, 99, 235);
const glm::vec3 kOrange(234, 88, 12);
const glm::vec3 kGrey(150, 150, 150);
const glm::vec3 kInk(40, 40, 40);

}  // namespace

int main(int argc, char** argv) {
  const char* out = (argc > 1) ? argv[1] : "docs/images/malley-hexagon.png";

  // Fresh Rng(1) per lobe, so each row of the post's table reproduces exactly.
  Lobe disk, sum, hexagon;
  double worst_len = 0.0;
  {
    Rng rng(1u);
    for (long i = 0; i < kSamples; ++i) {
      const glm::vec2 d = SampleUnitDisk(rng);
      const float z = Lift(d);
      disk.Add(z);
      const double len = std::sqrt(static_cast<double>(d.x) * d.x +
                                   static_cast<double>(d.y) * d.y +
                                   static_cast<double>(z) * z);
      worst_len = std::fmax(worst_len, std::fabs(len - 1.0));
    }
  }
  {
    Rng rng(1u);
    const glm::vec3 n(0.0f, 0.0f, 1.0f);
    for (long i = 0; i < kSamples; ++i) {
      sum.Add(glm::normalize(n + RandomUnitVector(rng)).z);
    }
  }
  {
    Rng rng(1u);
    for (long i = 0; i < kSamples; ++i) hexagon.Add(Lift(SampleUnitHexagon(rng)));
  }

  const double sigma = std::sqrt(0.5 - 4.0 / 9.0);  // of cos under the lobe
  const double std_err = sigma / std::sqrt(static_cast<double>(kSamples));
  std::printf("                     E[cos]    E[cos^2]\n");
  std::printf("disk lift            %.5f   %.5f\n", disk.Mean(), disk.MeanSq());
  std::printf("normalize(n + RUV)   %.5f   %.5f\n", sum.Mean(), sum.MeanSq());
  std::printf("hexagon lift         %.5f   %.5f\n", hexagon.Mean(),
              hexagon.MeanSq());
  std::printf("expected             %.5f   %.5f\n", 2.0 / 3.0, 0.5);
  std::printf("disk vs sum: %.5f apart; hexagon off truth by %.1f sigma\n",
              std::fabs(disk.Mean() - sum.Mean()),
              std::fabs(hexagon.Mean() - 2.0 / 3.0) / std_err);
  std::printf("disk lift max |length - 1|: %.3g\n", worst_len);

  //-------------------------------------------------------------------
  Canvas c(1000, 440);

  // Left: the apertures. Unit circle centred at (220, 220), radius 180 px.
  const glm::vec2 centre(220.0f, 220.0f);
  const float radius = 180.0f;
  for (int y = 0; y < c.h; ++y) {
    for (int x = 0; x < 440; ++x) {
      const float u = (x + 0.5f - centre.x) / radius;
      const float v = (centre.y - (y + 0.5f)) / radius;
      if (u * u + v * v <= 1.0f) c.Blend(x, y, kBlue, 0.18f);
      if (InHexagon(u, v)) c.Blend(x, y, kOrange, 0.28f);
    }
  }
  const int kArc = 256;
  for (int i = 0; i < kArc; ++i) {
    const float a0 = 2.0f * kPI * i / kArc, a1 = 2.0f * kPI * (i + 1) / kArc;
    c.Segment(centre + radius * glm::vec2(std::cos(a0), -std::sin(a0)),
              centre + radius * glm::vec2(std::cos(a1), -std::sin(a1)), 2.5f,
              kBlue);
  }
  for (int i = 0; i < 6; ++i) {
    const float a0 = kPI / 3.0f * i, a1 = kPI / 3.0f * (i + 1);
    c.Segment(centre + radius * glm::vec2(std::cos(a0), -std::sin(a0)),
              centre + radius * glm::vec2(std::cos(a1), -std::sin(a1)), 2.5f,
              kOrange);
  }

  // Right: density of cos(theta). x in [0, 1], y in [0, 2.6].
  const float px0 = 500.0f, px1 = 960.0f, py0 = 40.0f, py1 = 400.0f;
  auto to_px = [&](double cx, double dy) {
    return glm::vec2(px0 + static_cast<float>(cx) * (px1 - px0),
                     py1 - static_cast<float>(dy / 2.6) * (py1 - py0));
  };
  c.Segment(to_px(0, 0), to_px(1, 0), 2.0f, kInk);    // x axis
  c.Segment(to_px(0, 0), to_px(0, 2.6), 2.0f, kInk);  // y axis
  for (int t = 1; t <= 4; ++t) {                      // ticks at 0.25 steps
    c.Segment(to_px(t * 0.25, 0), to_px(t * 0.25, 0) + glm::vec2(0, 8), 2.0f,
              kInk);
  }
  c.Segment(to_px(0, 0), to_px(1, 2), 3.0f, kGrey);  // exact: p(cos) = 2 cos

  auto steps = [&](const Lobe& lobe, const glm::vec3& colour) {
    for (int b = 0; b < 20; ++b) {
      const double lo = b / 20.0, hi = (b + 1) / 20.0, d = lobe.Density(b);
      c.Segment(to_px(lo, d), to_px(hi, d), 3.5f, colour);
      if (b < 19) {
        c.Segment(to_px(hi, d), to_px(hi, lobe.Density(b + 1)), 3.5f, colour);
      }
    }
  };
  steps(hexagon, kOrange);
  steps(disk, kBlue);

  if (lodepng::encode(out, c.px, c.w, c.h) != 0) {
    std::fprintf(stderr, "could not write %s\n", out);
    return 1;
  }
  std::printf("wrote %s\n", out);
  return 0;
}
