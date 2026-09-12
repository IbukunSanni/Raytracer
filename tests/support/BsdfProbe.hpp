// The three ways to interrogate a BSDF, factored out of the test bodies so
// each test reads as a claim about a material rather than as a loop.
//
// Convention, matching the material headers: `in` and `out` are unit
// vectors that both point AWAY from the surface. The surface normal is +z
// throughout, so an incident angle is just a rotation in the xz plane.

#pragma once

#include "support/Statistics.hpp"

#include "render/Sampling.hpp"
#include "scene/Material.hpp"

namespace probe {

// Samples per estimate. Large enough that the standard error is a few
// parts in ten thousand, which is what makes the derived tolerances tight
// enough to catch a wrong normalisation factor.
constexpr int kSamples = 2000000;

// The surface normal every test is written against.
const glm::vec3 kNormal(0.0f, 0.0f, 1.0f);

// An incident direction at `degrees` from the normal, in the xz plane.
inline glm::vec3 incident(float degrees)
{
	const float theta = glm::radians(degrees);
	return glm::vec3(std::sin(theta), 0.0f, std::cos(theta));
}

// Uniform on the hemisphere around the normal. p(w) = 1/(2*pi).
inline glm::vec3 uniformHemisphere(Rng & rng)
{
	const float z = rng.next();
	const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
	const float phi = 2.0f * kPI * rng.next();
	return glm::vec3(r * std::cos(phi), r * std::sin(phi), z);
}

//---------------------------------------------------------------------
// Directional albedo: rho = integral of f_r * cos(theta) dw.
//
// Estimated with UNIFORM hemisphere samples, so the material's own pdf and
// sampler stay out of the loop and this measures eval() against nothing
// but the geometry. rho == 1 for a white Lambertian is the furnace
// condition; rho <= 1 for anything else is energy conservation.
inline stats::Estimate3 directionalAlbedo(const Material & mat,
                                          const glm::vec3 & in,
                                          uint32_t seed)
{
	Rng rng(seed);
	stats::Estimate3 rho;
	for (int i = 0; i < kSamples; ++i) {
		const glm::vec3 w = uniformHemisphere(rng);
		rho.add(mat.eval(in, kNormal, w) * glm::dot(kNormal, w) * (2.0f * kPI));
	}
	return rho;
}

//---------------------------------------------------------------------
// Everything sample() and pdf() disagreeing about would change, gathered
// in one pass so a test can assert on whichever part it means.
//
// The obvious estimator, the mean of f*cos/p, is DEGENERATE here: for a
// cosine-sampled Lambertian the pi and the cosine cancel algebraically, so
// it returns the albedo on every draw even if sample() returns garbage.
// Neither quantity below has that cancellation.
struct SamplerAgreement {
	// Integral of pdf() over the upper hemisphere. Must equal the fraction
	// of sample() draws that land there: both are the sampler's total mass
	// above the horizon, measured from opposite ends. For a material that
	// never scatters downwards both are 1; for one that does, neither is,
	// and they still have to match.
	stats::Estimate pdfMass;
	stats::Estimate fractionAbove;

	// A test function the pdf is NOT proportional to, so nothing cancels.
	// Integral of cos^2(theta) dw over the hemisphere is 2*pi/3, and a
	// wrong Jacobian in sample() or pdf() moves this and not the pair
	// above.
	stats::Estimate cosSquaredIntegral;
};

inline SamplerAgreement measureSampler(const Material & mat,
                                       const glm::vec3 & in,
                                       uint32_t seed)
{
	Rng rng(seed);
	SamplerAgreement m;

	for (int i = 0; i < kSamples; ++i) {
		const glm::vec3 w = uniformHemisphere(rng);
		m.pdfMass.add((double) mat.pdf(in, kNormal, w) * (2.0 * kPI));

		float pdf = 0.0f;
		glm::vec3 brdf(0.0f);
		const glm::vec3 out = mat.sample(rng, in, kNormal, &pdf, &brdf);
		const float cosOut = glm::dot(kNormal, out);

		m.fractionAbove.add(cosOut > 0.0f ? 1.0 : 0.0);
		m.cosSquaredIntegral.add(
		    pdf > 0.0f ? (double) (cosOut * cosOut) / (double) pdf : 0.0);
	}
	return m;
}

//---------------------------------------------------------------------
// One draw from a delta material. There is nothing to average: a delta
// lobe returns the same weight on every draw, so a single sample either
// satisfies the contract exactly or does not.
struct Draw {
	glm::vec3 direction;
	float pdf = 0.0f;
	glm::vec3 brdf{0.0f};
};

inline Draw drawOnce(const Material & mat, const glm::vec3 & in, uint32_t seed)
{
	Rng rng(seed);
	Draw d;
	d.direction = mat.sample(rng, in, kNormal, &d.pdf, &d.brdf);
	return d;
}

} // namespace probe
