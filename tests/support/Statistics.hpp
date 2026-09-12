// Statistical assertions for Monte Carlo tests.
//
// A BSDF check integrates a random estimator, so its result is never the
// exact answer. Comparing against a hand-picked tolerance means guessing:
// too tight and the suite flickers, too loose and a real bug walks past.
//
// Instead every estimator reports the standard error of its own run and the
// tolerance is derived from it. A correct estimator lands within four
// standard errors essentially always, while a wrong one is off by an amount
// that does not shrink however many samples you throw at it.

#pragma once

#include <doctest/doctest.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

// So a failed CHECK on a colour or a direction prints the numbers rather
// than doctest's "{?}" placeholder.
namespace doctest {
template <> struct StringMaker<glm::vec3> {
	static String convert(const glm::vec3 & v)
	{
		return String("(") + doctest::toString(v.x) + ", " +
		       doctest::toString(v.y) + ", " + doctest::toString(v.z) + ")";
	}
};
} // namespace doctest

namespace stats {

// Tolerance, in standard errors of the estimate.
constexpr double kSigmas = 4.0;

// Floor for the tolerance, so an estimator with no variance -- a single
// exact sample -- compares at float precision instead of demanding bit
// equality.
constexpr double kFloor = 1e-6;

//---------------------------------------------------------------------
// Welford's online mean and variance: one pass, no stored samples, and
// numerically stable where a running sum of squares is not.
class Estimate {
public:
	void add(double x)
	{
		++m_n;
		const double delta = x - m_mean;
		m_mean += delta / (double) m_n;
		m_m2 += delta * (x - m_mean);
	}

	double mean() const { return m_mean; }

	// The spread of this estimate, not of the samples it averaged. It
	// shrinks as 1/sqrt(n), which is what lets the tolerance below tighten
	// with the sample count rather than be guessed up front.
	double stdErr() const
	{
		return (m_n < 2) ? 0.0
		                 : std::sqrt(m_m2 / (double) (m_n - 1) / (double) m_n);
	}

	double tolerance() const { return std::max(kSigmas * stdErr(), kFloor); }

private:
	long long m_n = 0;
	double m_mean = 0.0;
	double m_m2 = 0.0;
};

// Three estimates run together, one per colour channel.
struct Estimate3 {
	Estimate channel[3];

	void add(const glm::vec3 & v)
	{
		channel[0].add(v.x);
		channel[1].add(v.y);
		channel[2].add(v.z);
	}
};

// Combined error of two independent estimates, for comparing them to each
// other rather than to a known value.
inline double jointTolerance(const Estimate & a, const Estimate & b)
{
	const double sa = a.stdErr(), sb = b.stdErr();
	return std::max(kSigmas * std::sqrt(sa * sa + sb * sb), kFloor);
}

} // namespace stats

//---------------------------------------------------------------------
// The assertions.
//
// CHECK, never REQUIRE: one wrong material should not suppress the verdict
// on the others. Each logs the numbers first, so a failure reads as "got
// this, wanted that, allowed this much" without opening the source.

#define CHECK_ESTIMATE(estimate, expected)                                    \
	do {                                                                      \
		const stats::Estimate & est_ = (estimate);                            \
		const double target_ = (double) (expected);                           \
		INFO("got ", est_.mean(), "  expected ", target_, "  +/- ",           \
		     est_.tolerance());                                               \
		CHECK(std::fabs(est_.mean() - target_) <= est_.tolerance());          \
	} while (false)

// Energy conservation is an inequality: a dark material passes as readily
// as a bright one, and only a material that reflects more than it received
// fails.
#define CHECK_ESTIMATE_AT_MOST(estimate, bound)                               \
	do {                                                                      \
		const stats::Estimate & est_ = (estimate);                            \
		const double limit_ = (double) (bound);                               \
		INFO("got ", est_.mean(), "  allowed <= ", limit_, "  +/- ",          \
		     est_.tolerance());                                               \
		CHECK(est_.mean() <= limit_ + est_.tolerance());                      \
	} while (false)

// Two estimators of the same integral agree. Neither side is known in
// closed form -- that they match is the whole claim.
#define CHECK_ESTIMATES_AGREE(a, b)                                           \
	do {                                                                      \
		const stats::Estimate & a_ = (a);                                     \
		const stats::Estimate & b_ = (b);                                     \
		const double tol_ = stats::jointTolerance(a_, b_);                    \
		INFO("got ", a_.mean(), "  vs ", b_.mean(), "  +/- ", tol_);          \
		CHECK(std::fabs(a_.mean() - b_.mean()) <= tol_);                      \
	} while (false)

// Per-channel CHECK_ESTIMATE against a colour. Three CHECKs rather than
// three SUBCASEs: a SUBCASE re-runs the enclosing body, and the body here
// is a million-sample integration.
#define CHECK_ESTIMATE3(estimate3, expectedColour)                            \
	do {                                                                      \
		const glm::vec3 colour_ = (expectedColour);                           \
		CHECK_ESTIMATE((estimate3).channel[0], colour_.r);                    \
		CHECK_ESTIMATE((estimate3).channel[1], colour_.g);                    \
		CHECK_ESTIMATE((estimate3).channel[2], colour_.b);                    \
	} while (false)
