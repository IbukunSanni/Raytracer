// Furnace test -- the BSDFs, checked in isolation.
//
// No scene, no renderer, no image. Each check integrates a BSDF over the
// hemisphere by Monte Carlo and compares against a value known in closed
// form. Tolerances are 4 standard errors of the estimator itself, so a
// failure means the BSDF is wrong rather than the sample count unlucky.
//
// Convention, matching the material headers: `in` and `out` are unit
// vectors that both point AWAY from the surface.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

#include "render/Sampling.hpp"
#include "scene/BlinnPhongMaterial.hpp"
#include "scene/LambertianMaterial.hpp"

namespace {

const int    kSamples = 2000000;
const double kSigmas  = 4.0;      // tolerance, in standard errors
const glm::vec3 kN(0.0f, 0.0f, 1.0f);

int g_failures = 0;

// Welford. Each check reports the standard error of its own estimator, so
// the tolerance is derived from the run instead of guessed up front.
class Accum {
public:
	void add(double x) {
		++m_n;
		const double d = x - m_mean;
		m_mean += d / (double) m_n;
		m_m2   += d * (x - m_mean);
	}
	double mean() const { return m_mean; }
	double stdErr() const {
		return (m_n < 2) ? 0.0
		                 : std::sqrt(m_m2 / (double)(m_n - 1) / (double) m_n);
	}
private:
	long long m_n = 0;
	double m_mean = 0.0;
	double m_m2 = 0.0;
};

struct Accum3 {
	Accum c[3];
	void add(const glm::vec3 & v) { c[0].add(v.x); c[1].add(v.y); c[2].add(v.z); }
};

void report(bool ok, const std::string & what, double got, const char * expect) {
	std::printf("  %s  %-46s %9.6f   %s\n", ok ? "PASS" : "FAIL",
	            what.c_str(), got, expect);
	if (!ok) ++g_failures;
}

void check(const std::string & what, const Accum & a, double expected) {
	const double tol = std::max(kSigmas * a.stdErr(), 1e-6);
	char e[64];
	std::snprintf(e, sizeof e, "expect %.6f +/- %.6f", expected, tol);
	report(std::fabs(a.mean() - expected) <= tol, what, a.mean(), e);
}

void checkAtMost(const std::string & what, const Accum & a, double bound) {
	const double tol = std::max(kSigmas * a.stdErr(), 1e-6);
	char e[64];
	std::snprintf(e, sizeof e, "expect <= %.4f (+/- %.6f)", bound, tol);
	report(a.mean() <= bound + tol, what, a.mean(), e);
}

// Two estimators of the same integral must agree within their combined error.
void checkAgree(const std::string & what, const Accum & a, const Accum & b) {
	const double sa = a.stdErr(), sb = b.stdErr();
	const double tol = std::max(kSigmas * std::sqrt(sa * sa + sb * sb), 1e-6);
	char e[80];
	std::snprintf(e, sizeof e, "vs %.6f, gap tol %.6f", b.mean(), tol);
	report(std::fabs(a.mean() - b.mean()) <= tol, what, a.mean(), e);
}

// Uniform on the hemisphere around +z. p(w) = 1/(2*pi).
glm::vec3 uniformHemisphere(Rng & rng) {
	const float z   = rng.next();
	const float r   = std::sqrt(std::max(0.0f, 1.0f - z * z));
	const float phi = 2.0f * kPI * rng.next();
	return glm::vec3(r * std::cos(phi), r * std::sin(phi), z);
}

glm::vec3 incident(float degrees) {
	const float t = glm::radians(degrees);
	return glm::vec3(std::sin(t), 0.0f, std::cos(t));
}

//---------------------------------------------------------------------
// Directional albedo: rho = integral of f_r * cos(theta) dw.
//
// Estimated with UNIFORM hemisphere samples, so the material's own pdf and
// sampler are not in the loop -- this checks eval() against nothing but the
// geometry. rho == 1 for a white Lambertian is the furnace condition; rho
// <= 1 for anything else is energy conservation.
Accum3 albedo(const Material & mat, const glm::vec3 & in, uint32_t seed) {
	Rng rng(seed);
	Accum3 a;
	for (int i = 0; i < kSamples; ++i) {
		const glm::vec3 w = uniformHemisphere(rng);
		a.add(mat.eval(in, kN, w) * glm::dot(kN, w) * (2.0f * kPI));
	}
	return a;
}

//---------------------------------------------------------------------
// sample() against pdf(), without the cancellation trap.
//
// The obvious estimator (1/N) * sum(f*cos/p) is DEGENERATE for a cosine-
// sampled Lambertian: the pi and the cosine cancel algebraically, so it
// returns the albedo on every sample even if sample() draws garbage. These
// two checks have no such cancellation.
//
//   (a) Mass above the horizon. The integral of pdf over the upper
//       hemisphere must equal the fraction of sample() draws that land
//       there. A specular lobe can reflect below the surface, where pdf()
//       returns 0, so neither side is 1 -- but they must be the same number.
//
//   (b) A test function the pdf is not proportional to. Both estimators
//       target integral of cos^2(theta) dw = 2*pi/3: one uniform, one
//       importance-sampled through sample()/pdf(). A wrong Jacobian in the
//       specular pdf moves the second and not the first.
void checkSampler(const std::string & name, const Material & mat,
                  const glm::vec3 & in, uint32_t seed) {
	Rng rng(seed);
	Accum pdfMass, aboveFrac, gImportance;

	for (int i = 0; i < kSamples; ++i) {
		const glm::vec3 w = uniformHemisphere(rng);
		pdfMass.add((double) mat.pdf(in, kN, w) * (2.0 * kPI));

		float pdf = 0.0f;
		glm::vec3 brdf(0.0f);
		const glm::vec3 out = mat.sample(rng, in, kN, &pdf, &brdf);
		const float cosOut = glm::dot(kN, out);

		aboveFrac.add(cosOut > 0.0f ? 1.0 : 0.0);
		gImportance.add(pdf > 0.0f ? (double)(cosOut * cosOut) / (double) pdf
		                           : 0.0);
	}

	checkAgree(name + ": pdf mass == frac above horizon", pdfMass, aboveFrac);
	check(name + ": integral cos^2 dw via sample/pdf", gImportance, 2.0 * kPI / 3.0);
}

} // namespace

int main() {
	std::printf("furnace test -- %d samples per estimate, %.0f sigma tolerance\n\n",
	            kSamples, kSigmas);

	const glm::vec3 in0  = incident(0.0f);
	const glm::vec3 in60 = incident(60.0f);

	// --- 1. white furnace -------------------------------------------------
	// A Lambertian with albedo 1 reflects every photon it receives. If this
	// is not 1.0 the renderer cannot be energy-conserving no matter what the
	// integrator does.
	std::printf("Lambertian, albedo 1\n");
	{
		LambertianMaterial white(glm::vec3(1.0f));
		Accum3 a = albedo(white, in0, 1u);
		check("white furnace: rho (r)", a.c[0], 1.0);
		check("white furnace: rho (g)", a.c[1], 1.0);
		check("white furnace: rho (b)", a.c[2], 1.0);

		// Malley's method: a uniform disk sample lifted to the hemisphere is
		// cosine-distributed, so E[cos] = integral of cos^2/pi dw = 2/3. The
		// same integral as E[sqrt(1-r^2)] over the disk, which is why the
		// aperture sampler and the hemisphere sampler are one function.
		Rng rng(2u);
		Accum m;
		for (int i = 0; i < kSamples; ++i) {
			float pdf;
			glm::vec3 brdf;
			m.add((double) glm::dot(kN, white.sample(rng, in0, kN, &pdf, &brdf)));
		}
		check("cosine sampler: E[cos]", m, 2.0 / 3.0);
	}
	{
		LambertianMaterial grey(glm::vec3(0.8f));
		checkSampler("Lambertian", grey, in0, 3u);
	}

	// --- 2. coloured Lambertian ------------------------------------------
	std::printf("\nLambertian, albedo (0.9, 0.5, 0.2)\n");
	{
		LambertianMaterial col(glm::vec3(0.9f, 0.5f, 0.2f));
		Accum3 a = albedo(col, in60, 4u);
		check("rho (r)", a.c[0], 0.9);
		check("rho (g)", a.c[1], 0.5);
		check("rho (b)", a.c[2], 0.2);
	}

	// --- 3. Blinn-Phong energy conservation ------------------------------
	// kd + ks <= 1 must give rho <= 1 at every angle of incidence and every
	// exponent. An un-normalised specular lobe fails here, brightly.
	std::printf("\nBlinn-Phong, kd + ks <= 1\n");
	{
		struct Case { float kd, ks; double n; const char * label; };
		const Case cases[] = {
			{0.6f, 0.3f,   1.0, "kd .6 ks .3 n 1"},
			{0.6f, 0.3f,  25.0, "kd .6 ks .3 n 25"},
			{0.2f, 0.7f, 200.0, "kd .2 ks .7 n 200"},
			{0.0f, 0.9f,  50.0, "kd 0  ks .9 n 50"},
			{0.9f, 0.0f,  50.0, "kd .9 ks 0  n 50"},
		};
		uint32_t seed = 10u;
		for (const Case & c : cases) {
			BlinnPhongMaterial m(glm::vec3(c.kd), glm::vec3(c.ks), c.n);
			const float degrees[] = {0.0f, 45.0f, 80.0f};
			for (float deg : degrees) {
				Accum3 a = albedo(m, incident(deg), seed++);
				char label[96];
				std::snprintf(label, sizeof label, "%s @ %.0f deg", c.label, deg);
				checkAtMost(label, a.c[0], 1.0);
			}
		}
	}

	// --- 4. Blinn-Phong sampler ------------------------------------------
	std::printf("\nBlinn-Phong sampler vs pdf\n");
	{
		BlinnPhongMaterial m(glm::vec3(0.5f), glm::vec3(0.4f), 25.0);
		checkSampler("n 25 @ 0 deg",  m, incident(0.0f),  40u);
		checkSampler("n 25 @ 45 deg", m, incident(45.0f), 41u);
		checkSampler("n 25 @ 80 deg", m, incident(80.0f), 42u);

		BlinnPhongMaterial sharp(glm::vec3(0.3f), glm::vec3(0.6f), 200.0);
		checkSampler("n 200 @ 45 deg", sharp, incident(45.0f), 43u);
	}

	std::printf("\n%s\n", g_failures == 0 ? "all furnace checks passed"
	                                      : "FURNACE CHECKS FAILED");
	return g_failures;
}
