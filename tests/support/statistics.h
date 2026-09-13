// Statistical assertions for Monte Carlo tests.
//
// A test that integrates a random estimator never gets the exact answer,
// so it needs a tolerance. Picking one by hand is a guess: too tight and
// the test flickers, too loose and a real bug walks past.
//
// So each estimator reports the standard error of its own run, and the
// tolerance comes from that. A correct estimator lands within four
// standard errors essentially always. A wrong one is off by an amount that
// does not shrink however many samples you throw at it.

#ifndef RAYTRACER_TESTS_SUPPORT_STATISTICS_H_
#define RAYTRACER_TESTS_SUPPORT_STATISTICS_H_

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

// So a failed check on a colour or a direction prints the numbers instead
// of doctest's "{?}" placeholder.
namespace doctest {
template <>
struct StringMaker<glm::vec3> {
  // doctest looks this up by the exact spelling `convert`, so the Google
  // rule does not apply: renaming it to Convert compiles and then silently
  // falls back to printing "{?}".
  // NOLINTNEXTLINE(readability-identifier-naming)
  static String convert(const glm::vec3& v) {
    return String("(") + doctest::toString(v.x) + ", " +
           doctest::toString(v.y) + ", " + doctest::toString(v.z) + ")";
  }
};
}  // namespace doctest

namespace stats {

// Tolerance, in standard errors of the estimate.
constexpr double kSigmas = 4.0;

// Smallest tolerance allowed, so an estimator with no variance compares at
// float precision rather than demanding bit equality.
constexpr double kFloor = 1e-6;

// Welford's online mean and variance: one pass, no stored samples, and
// stable where a running sum of squares is not.
class Estimate {
 public:
  void Add(double x) {
    ++n_;
    const double delta = x - mean_;
    mean_ += delta / static_cast<double>(n_);
    m2_ += delta * (x - mean_);
  }

  double Mean() const { return mean_; }

  // The spread of this estimate, not of the samples it averaged. It
  // shrinks as 1/sqrt(n), which is what lets the tolerance tighten with
  // the sample count instead of being fixed up front.
  double StdErr() const {
    return (n_ < 2) ? 0.0
                    : std::sqrt(m2_ / static_cast<double>(n_ - 1) /
                                static_cast<double>(n_));
  }

  double Tolerance() const { return std::max(kSigmas * StdErr(), kFloor); }

 private:
  long long n_ = 0;
  double mean_ = 0.0;
  double m2_ = 0.0;
};

// Three estimates run together, one per colour channel.
struct Estimate3 {
  Estimate channel[3];

  void Add(const glm::vec3& v) {
    channel[0].Add(v.x);
    channel[1].Add(v.y);
    channel[2].Add(v.z);
  }
};

// Combined error of two estimates, for comparing them to each other rather
// than to a known value.
inline double JointTolerance(const Estimate& a, const Estimate& b) {
  const double sa = a.StdErr(), sb = b.StdErr();
  return std::max(kSigmas * std::sqrt(sa * sa + sb * sb), kFloor);
}

}  // namespace stats

//---------------------------------------------------------------------
// The assertions.
//
// CHECK rather than REQUIRE, so one wrong material does not suppress the
// verdict on the rest. Each logs its numbers first, so a failure reads as
// "got this, wanted that, allowed this much" without opening the source.
//
// Macros rather than functions: a helper function would report its own
// line as the failure site instead of the caller's.

#define CHECK_ESTIMATE(estimate, expected)                       \
  do {                                                           \
    const stats::Estimate& est_ = (estimate);                    \
    const double target_ = static_cast<double>(expected);        \
    INFO("got ", est_.Mean(), "  expected ", target_, "  +/- ",  \
         est_.Tolerance());                                      \
    CHECK(std::fabs(est_.Mean() - target_) <= est_.Tolerance()); \
  } while (false)

// An upper bound, for claims that are inequalities. A dark material passes
// as readily as a bright one; only one that returns more than it received
// fails.
#define CHECK_ESTIMATE_AT_MOST(estimate, bound)                  \
  do {                                                           \
    const stats::Estimate& est_ = (estimate);                    \
    const double limit_ = static_cast<double>(bound);            \
    INFO("got ", est_.Mean(), "  allowed <= ", limit_, "  +/- ", \
         est_.Tolerance());                                      \
    CHECK(est_.Mean() <= limit_ + est_.Tolerance());             \
  } while (false)

// Two estimates of the same quantity match. Neither side is known in
// closed form; that they agree is the whole claim.
#define CHECK_ESTIMATES_AGREE(a, b)                              \
  do {                                                           \
    const stats::Estimate& a_ = (a);                             \
    const stats::Estimate& b_ = (b);                             \
    const double tol_ = stats::JointTolerance(a_, b_);           \
    INFO("got ", a_.Mean(), "  vs ", b_.Mean(), "  +/- ", tol_); \
    CHECK(std::fabs(a_.Mean() - b_.Mean()) <= tol_);             \
  } while (false)

// Per-channel CHECK_ESTIMATE against a colour. Three checks rather than
// three doctest SUBCASEs, because a SUBCASE re-runs the enclosing body and
// the body here is a long integration.
#define CHECK_ESTIMATE3(estimate3, expectedColour)     \
  do {                                                 \
    const glm::vec3 colour_ = (expectedColour);        \
    CHECK_ESTIMATE((estimate3).channel[0], colour_.r); \
    CHECK_ESTIMATE((estimate3).channel[1], colour_.g); \
    CHECK_ESTIMATE((estimate3).channel[2], colour_.b); \
  } while (false)

#endif  // RAYTRACER_TESTS_SUPPORT_STATISTICS_H_
