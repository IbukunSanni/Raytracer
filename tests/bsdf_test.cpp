// The materials, checked on their own -- no scene, no renderer, no image.
//
// Three claims, applied to each material in turn.
//
//   1. ENERGY. A surface cannot return more light than it received. Its
//      directional albedo is at most 1, and exactly 1 for a white diffuse,
//      the one case with an answer in closed form.
//
//   2. SAMPLING. sample() and pdf() must describe the same distribution.
//      They are written separately and used together in a product where a
//      matched pair of errors cancels silently, so they are compared here
//      where nothing can cancel.
//
//   3. THE DELTA CONTRACT. A mirror scatters into a single direction, so
//      it carries no density. eval() and pdf() are zero, sample() reports
//      a pdf of 1, and the weight it hands back is applied as-is by
//      whoever called it -- which is why that weight must be exact rather
//      than merely close.

#include "support/BsdfProbe.hpp"

#include "scene/Material.hpp"
#include "scene/Scattering.hpp"

using probe::incident;
using probe::kNormal;

//=====================================================================
TEST_SUITE("bsdf/lambertian")
{

TEST_CASE("lambertian: albedo 1 returns every photon it receives")
{
	const LambertianMaterial white(glm::vec3(1.0f));
	stats::Estimate3 rho = probe::directionalAlbedo(white, incident(0.0f), 1u);
	CHECK_ESTIMATE3(rho, glm::vec3(1.0f));
}

TEST_CASE("lambertian: albedo is reproduced per channel at a grazing angle")
{
	// A diffuse lobe is constant in every direction, so the angle of
	// incidence must not change what comes back.
	const glm::vec3 albedo(0.9f, 0.5f, 0.2f);
	const LambertianMaterial coloured(albedo);
	stats::Estimate3 rho = probe::directionalAlbedo(coloured, incident(60.0f), 4u);
	CHECK_ESTIMATE3(rho, albedo);
}

TEST_CASE("lambertian: the sampler really is cosine distributed")
{
	// A uniform disk sample lifted to the hemisphere is cosine
	// distributed, so E[cos] = integral of cos^2/pi dw = 2/3.
	const LambertianMaterial white(glm::vec3(1.0f));
	Rng rng(2u);
	stats::Estimate meanCosine;

	for (int i = 0; i < probe::kSamples; ++i) {
		float pdf;
		glm::vec3 brdf;
		const glm::vec3 out = white.sample(rng, incident(0.0f), kNormal, &pdf, &brdf);
		meanCosine.add((double) glm::dot(kNormal, out));
	}
	CHECK_ESTIMATE(meanCosine, 2.0 / 3.0);
}

TEST_CASE("lambertian: sample() and pdf() describe the same distribution")
{
	const LambertianMaterial grey(glm::vec3(0.8f));
	probe::SamplerAgreement m = probe::measureSampler(grey, incident(0.0f), 3u);

	CHECK_ESTIMATES_AGREE(m.pdfMass, m.fractionAbove);
	CHECK_ESTIMATE(m.cosSquaredIntegral, 2.0 * kPI / 3.0);
}

} // TEST_SUITE bsdf/lambertian

//=====================================================================
TEST_SUITE("bsdf/blinn-phong")
{

TEST_CASE("blinn-phong: kd + ks <= 1 conserves energy at every angle")
{
	// An un-normalised specular lobe fails this, brightly. The exponent
	// sets how wide the lobe is and the normalisation factor has to track
	// it, so the cases sweep both the split between the two lobes and how
	// tight the specular one is.
	struct Case {
		float kd, ks;
		double exponent;
	};
	const Case cases[] = {
	    {0.6f, 0.3f, 1.0},   // the widest specular lobe
	    {0.6f, 0.3f, 25.0},  // a typical highlight
	    {0.2f, 0.7f, 200.0}, // specular-dominant and tight
	    {0.0f, 0.9f, 50.0},  // specular only
	    {0.9f, 0.0f, 50.0},  // diffuse only
	};
	const float angles[] = {0.0f, 45.0f, 80.0f};

	uint32_t seed = 10u;
	for (const Case & c : cases) {
		const BlinnPhongMaterial m(glm::vec3(c.kd), glm::vec3(c.ks), c.exponent);
		for (float degrees : angles) {
			CAPTURE(c.kd);
			CAPTURE(c.ks);
			CAPTURE(c.exponent);
			CAPTURE(degrees);
			stats::Estimate3 rho =
			    probe::directionalAlbedo(m, incident(degrees), seed++);
			CHECK_ESTIMATE_AT_MOST(rho.channel[0], 1.0);
		}
	}
}

TEST_CASE("blinn-phong: sample() and pdf() agree at every incident angle")
{
	// This lobe depends on where the light came from, so the pair has to
	// agree at every angle rather than only head-on. At 80 degrees most of
	// the lobe hangs below the horizon, which is where a sampler that
	// forgets to reject those draws parts company with its pdf.
	struct Case {
		const char * label;
		float kd, ks;
		double exponent;
		float degrees;
	};
	const Case cases[] = {
	    {"n 25, head-on", 0.5f, 0.4f, 25.0, 0.0f},
	    {"n 25, 45 deg", 0.5f, 0.4f, 25.0, 45.0f},
	    {"n 25, grazing", 0.5f, 0.4f, 25.0, 80.0f},
	    {"n 200, 45 deg", 0.3f, 0.6f, 200.0, 45.0f},
	};

	uint32_t seed = 40u;
	for (const Case & c : cases) {
		CAPTURE(c.label);
		const BlinnPhongMaterial m(glm::vec3(c.kd), glm::vec3(c.ks), c.exponent);
		probe::SamplerAgreement s =
		    probe::measureSampler(m, incident(c.degrees), seed++);

		CHECK_ESTIMATES_AGREE(s.pdfMass, s.fractionAbove);
		CHECK_ESTIMATE(s.cosSquaredIntegral, 2.0 * kPI / 3.0);
	}
}

} // TEST_SUITE bsdf/blinn-phong

//=====================================================================
// Mirror and metal are the same delta lobe, so they answer to the same
// contract and share the helper that states it.

namespace {

void checkDeltaContract(const Material & mat,
                        const glm::vec3 & in,
                        const glm::vec3 & expectedAlbedo,
                        uint32_t seed)
{
	const probe::Draw d = probe::drawOnce(mat, in, seed);

	CHECK(mat.isSpecular());
	CHECK(d.pdf == 1.0f);

	// Exact, not approximate: the caller multiplies this weight straight
	// into a running product, so drift here is drift on every bounce.
	CHECK(d.brdf.r == expectedAlbedo.r);
	CHECK(d.brdf.g == expectedAlbedo.g);
	CHECK(d.brdf.b == expectedAlbedo.b);

	// No density to report, asked from either end.
	CHECK(mat.eval(in, kNormal, d.direction) == glm::vec3(0.0f));
	CHECK(mat.pdf(in, kNormal, d.direction) == 0.0f);
}

} // namespace

TEST_SUITE("bsdf/mirror")
{

TEST_CASE("mirror: the delta contract holds at any angle and any albedo")
{
	const MirrorMaterial white(glm::vec3(1.0f));
	checkDeltaContract(white, incident(0.0f), glm::vec3(1.0f), 50u);
	checkDeltaContract(white, incident(60.0f), glm::vec3(1.0f), 51u);

	const glm::vec3 tint(0.9f, 0.5f, 0.2f);
	checkDeltaContract(MirrorMaterial(tint), incident(0.0f), tint, 52u);
}

TEST_CASE("mirror: the scattered direction is the reflected direction")
{
	const MirrorMaterial white(glm::vec3(1.0f));
	const glm::vec3 in = incident(60.0f);
	CHECK(probe::drawOnce(white, in, 53u).direction == reflect(in, kNormal));
}

} // TEST_SUITE bsdf/mirror

//=====================================================================
TEST_SUITE("bsdf/metal")
{

// Metal perturbs the mirror direction, and absorbs the ray if the
// perturbation tipped it into the surface. Absorption is only reachable
// where the lobe straddles the horizon, so the cases below keep the two
// regimes apart: head-on, where nothing can be absorbed and the delta
// contract must hold on every draw, and grazing, where absorption is the
// behaviour under test.

TEST_CASE("metal: a scattered ray carries exactly the albedo, at any fuzz")
{
	// Head-on, so the whole lobe stays above the surface however wide it
	// is. Roughening the reflection must not change what the rays carry.
	const glm::vec3 tint(0.9f, 0.5f, 0.2f);
	const float radii[] = {0.0f, 0.4f, 0.9f};

	uint32_t seed = 60u;
	for (float fuzz : radii) {
		CAPTURE(fuzz);
		checkDeltaContract(MetalMaterial(tint, fuzz), incident(0.0f), tint, seed++);
	}
}

TEST_CASE("metal: fuzz 0 is a mirror, exactly")
{
	// The degenerate case has to collapse onto the material it
	// generalises, or the two are separate implementations of one physics.
	const glm::vec3 in = incident(60.0f);
	const MetalMaterial sharp(glm::vec3(1.0f), 0.0f);
	CHECK(probe::drawOnce(sharp, in, 62u).direction == reflect(in, kNormal));
}

TEST_CASE("metal: the lobe is a cone of half-angle asin(fuzz) about the mirror")
{
	// Adding fuzz times a unit vector to the unit mirror direction and
	// renormalising sweeps a cone, and the widest the sum can lean is
	// asin(fuzz). That bound is what makes the parameter mean something:
	// the reflection blurs by a predictable amount rather than an
	// arbitrary one.
	//
	// The lobe also has to be CENTRED on the mirror direction, so the
	// sideways components average out. A direction sampler that ever fell
	// back to a fixed vector would tilt the whole lobe that way while
	// leaving energy untouched and the blur intact, so this is the only
	// assertion that would notice.
	const glm::vec3 in = incident(30.0f);
	const glm::vec3 mirrorDirection = reflect(in, kNormal);
	const float fuzz = 0.3f;
	const MetalMaterial rough(glm::vec3(1.0f), fuzz);

	// Two directions across the lobe, to measure any lean in.
	const glm::vec3 sideways = glm::normalize(glm::cross(mirrorDirection, kNormal));
	const glm::vec3 upDown = glm::cross(sideways, mirrorDirection);

	const float widestLean = std::sqrt(1.0f - fuzz * fuzz); // cos(asin(fuzz))

	Rng rng(63u);
	stats::Estimate sidewaysMean, upDownMean;
	float worstLength = 0.0f;
	float worstLean = 1.0f;

	for (int i = 0; i < 200000; ++i) {
		float pdf;
		glm::vec3 brdf;
		const glm::vec3 out = rough.sample(rng, in, kNormal, &pdf, &brdf);

		worstLength = std::max(worstLength, std::fabs(glm::length(out) - 1.0f));
		worstLean = std::min(worstLean, glm::dot(out, mirrorDirection));
		sidewaysMean.add((double) glm::dot(out, sideways));
		upDownMean.add((double) glm::dot(out, upDown));
	}

	CHECK(worstLength < 1e-5f);             // it is a ray direction
	CHECK(worstLean >= widestLean - 1e-5f); // inside the cone, on every draw
	CHECK_ESTIMATE(sidewaysMean, 0.0);      // and centred in it
	CHECK_ESTIMATE(upDownMean, 0.0);
}

TEST_CASE("metal: a perturbation into the surface is absorbed")
{
	// At a grazing angle a wide lobe hangs partly below the horizon. Those
	// draws cannot be scattered, so the material reports no density and no
	// weight. This is the one case where a delta material returns a pdf of
	// 0, and it is deliberate: a rough metal darkens at its silhouette.
	const MetalMaterial rough(glm::vec3(1.0f), 0.8f);
	const glm::vec3 in = incident(80.0f);

	Rng rng(70u);
	int absorbed = 0;
	const int draws = 20000;

	for (int i = 0; i < draws; ++i) {
		float pdf;
		glm::vec3 brdf;
		const glm::vec3 out = rough.sample(rng, in, kNormal, &pdf, &brdf);

		if (glm::dot(kNormal, out) <= 0.0f) {
			++absorbed;
			CHECK(pdf == 0.0f);
			CHECK(brdf == glm::vec3(0.0f));
		} else {
			CHECK(pdf == 1.0f);
			CHECK(brdf == glm::vec3(1.0f));
		}
	}

	// The configuration has to reach both branches, or the assertions
	// above are checking nothing.
	CHECK(absorbed > 0);
	CHECK(absorbed < draws);
}

} // TEST_SUITE bsdf/metal
