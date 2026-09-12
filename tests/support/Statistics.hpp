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

#pragma once

#include <doctest/doctest.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

// So a failed check on a colour or a direction prints the numbers instead
// of doctest's "{?}" placeholder.
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

// Smallest tolerance allowed, so an estimator with no variance compares at
// float precision rather than demanding bit equality.
constexpr double kFloor = 1e-6;

// Welford's online mean and variance: one pass, no stored samples, and
// stable where a running sum of squares is not.
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
	// shrinks as 1/sqrt(n), which is what lets the tolerance tighten with
	// the sample count instead of being fixed up front.
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

// Combined error of two estimates, for comparing them to each other rather
// than to a known value.
inline double jointTolerance(const Estimate & a, const Estimate & b)
{
	const double sa = a.stdErr(), sb = b.stdErr();
	return std::max(kSigmas * std::sqrt(sa * sa + sb * sb), kFloor);
}

} // namespace stats

//---------------------------------------------------------------------
// The assertions.
//
// CHECK rather than REQUIRE, so one wrong material does not suppress the
// verdict on the rest. Each logs its numbers first, so a failure reads as
// "got this, wanted that, allowed this much" without opening the source.
//
// Macros rather than functions: a helper function would report its own
// line as the failure site instead of the caller's.

#define CHECK_ESTIMATE(estimate, expected)                                    \
	do {                                                                      \
		const stats::Estimate & est_ = (estimate);                            \
		const double target_ = (double) (expected);                           \
		INFO("got ", est_.mean(), "  expected ", target_, "  +/- ",           \
		     est_.tolerance());                                               \
		CHECK(std::fabs(est_.mean() - target_) <= est_.tolerance());          \
	} while (false)

// An upper bound, for claims that are inequalities. A dark material passes
// as readily as a bright one; only one that returns more than it received
// fails.
#define CHECK_ESTIMATE_AT_MOST(estimate, bound)                               \
	do {                                                                      \
		const stats::Estimate & est_ = (estimate);                            \
		const double limit_ = (double) (bound);                               \
		INFO("got ", est_.mean(), "  allowed <= ", limit_, "  +/- ",          \
		     est_.tolerance());                                               \
		CHECK(est_.mean() <= limit_ + est_.tolerance());                      \
	} while (false)

// Two estimates of the same quantity match. Neither side is known in
// closed form; that they agree is the whole claim.
#define CHECK_ESTIMATES_AGREE(a, b)                                           \
	do {                                                                      \
		const stats::Estimate & a_ = (a);                                     \
		const stats::Estimate & b_ = (b);                                     \
		const double tol_ = stats::jointTolerance(a_, b_);                    \
		INFO("got ", a_.mean(), "  vs ", b_.mean(), "  +/- ", tol_);          \
		CHECK(std::fabs(a_.mean() - b_.mean()) <= tol_);                      \
	} while (false)

// Per-channel CHECK_ESTIMATE against a colour. Three checks rather than
// three doctest SUBCASEs, because a SUBCASE re-runs the enclosing body and
// the body here is a long integration.
#define CHECK_ESTIMATE3(estimate3, expectedColour)                            \
	do {                                                                      \
		const glm::vec3 colour_ = (expectedColour);                           \
		CHECK_ESTIMATE((estimate3).channel[0], colour_.r);                    \
		CHECK_ESTIMATE((estimate3).channel[1], colour_.g);                    \
		CHECK_ESTIMATE((estimate3).channel[2], colour_.b);                    \
	} while (false)
