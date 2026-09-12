// The BSDFs, checked in isolation -- no scene, no renderer, no image.
//
// Every material has to satisfy three things, and this file is those three
// claims applied to each one in turn:
//
//   1. ENERGY. A surface cannot reflect more light than it receives. The
//      directional albedo, integral of f_r * cos dw, is at most 1, and
//      exactly 1 for a white Lambertian. That last case is the furnace
//      condition: it is the only material whose answer is known in closed
//      form, which is why it anchors the rest.
//
//   2. SAMPLING. sample() and pdf() must describe the same distribution.
//      They are written separately and used together, in a product where a
//      matched pair of errors cancels silently, so they are checked
//      against each other here where nothing can cancel.
//
//   3. THE DELTA CONTRACT. A mirror scatters into one direction, so it has
//      no density: eval() and pdf() are zero and sample() carries the
//      material entirely. The renderer branches on isSpecular() to apply
//      that weight unmodified, so the weight has to be exact.
//
// A failure here names a material. A failure in render_test.cpp names a
// picture, and could be the integrator instead.

#include "support/BsdfProbe.hpp"
#include "support/Statistics.hpp"

#include "scene/Material.hpp"
#include "scene/Scattering.hpp"

using probe::incident;
using probe::kNormal;

//=====================================================================
TEST_SUITE("bsdf/lambertian")
{

TEST_CASE("lambertian: albedo 1 reflects every photon it receives")
{
	// If this is not exactly 1 the renderer cannot conserve energy no
	// matter what the integrator does afterwards.
	const LambertianMaterial white(glm::vec3(1.0f));
	stats::Estimate3 rho = probe::directionalAlbedo(white, incident(0.0f), 1u);
	CHECK_ESTIMATE3(rho, glm::vec3(1.0f));
}

TEST_CASE("lambertian: albedo is reproduced per channel at a grazing angle")
{
	// A Lambertian is constant in every direction, so the angle of
	// incidence must not change what comes back.
	const glm::vec3 albedo(0.9f, 0.5f, 0.2f);
	const LambertianMaterial coloured(albedo);
	stats::Estimate3 rho = probe::directionalAlbedo(coloured, incident(60.0f), 4u);
	CHECK_ESTIMATE3(rho, albedo);
}

TEST_CASE("lambertian: the cosine sampler really is cosine distributed")
{
	// Malley's method: a uniform disk sample lifted to the hemisphere is
	// cosine distributed, so E[cos] = integral of cos^2/pi dw = 2/3. That
	// is the same integral as E[sqrt(1-r^2)] over the disk, which is why
	// the aperture sampler and the hemisphere sampler are one function.
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

TEST_CASE("blinn-phong: kd + ks <= 1 conserves energy at every angle and exponent")
{
	// An un-normalised specular lobe fails this, brightly. The exponent
	// changes the lobe's width, and the normalisation factor has to track
	// it, so the cases sweep both the split between the lobes and how
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
	// Unlike the Lambertian this lobe depends on the incident direction,
	// so the pair has to agree at every angle, not just head-on. At 80
	// degrees most of the lobe hangs below the horizon, which is where a
	// sampler that forgets to reject those draws diverges from its pdf.
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
// contract and are tested by the same helper.

namespace {

// The whole delta convention in one place: pdf is 1, brdf is the
// throughput weight the renderer applies unmodified, and the density
// functions are silent because there is no density to report.
void checkDeltaContract(const Material & mat,
                        const glm::vec3 & in,
                        const glm::vec3 & expectedAlbedo,
                        uint32_t seed)
{
	const probe::Draw d = probe::drawOnce(mat, in, seed);

	CHECK(mat.isSpecular());
	CHECK(d.pdf == 1.0f);

	// Exact, not approximate. The renderer multiplies this straight into
	// the throughput, so any drift here is drift in every bounce off the
	// material -- which is precisely how a mirror ended up one code darker
	// than its environment when the weight was divided and multiplied by a
	// cosine on the way through.
	CHECK(d.brdf.r == expectedAlbedo.r);
	CHECK(d.brdf.g == expectedAlbedo.g);
	CHECK(d.brdf.b == expectedAlbedo.b);

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

// Metal normalises the perturbed direction and then absorbs it if the
// perturbation tipped it into the surface. Absorption is only reachable
// where the lobe straddles the horizon, so the tests below separate the
// two regimes rather than mixing them: head-on, where nothing can be
// absorbed and the delta contract must hold on every draw, and grazing,
// where absorption is the behaviour under test.

TEST_CASE("metal: a scattered ray carries exactly the albedo, at any fuzz")
{
	// Head-on, so the whole lobe stays above the surface however wide it
	// is and no draw is absorbed. Roughening the reflection must not cost
	// or create energy in the rays that do leave.
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
	// generalises, or the two are separate implementations of the same
	// physics.
	const glm::vec3 in = incident(60.0f);
	const MetalMaterial sharp(glm::vec3(1.0f), 0.0f);
	CHECK(probe::drawOnce(sharp, in, 62u).direction == reflect(in, kNormal));
}

TEST_CASE("metal: the lobe is a cone of half-angle asin(fuzz) about the mirror")
{
	// Adding `fuzz` times a unit vector to the unit mirror direction and
	// renormalising sweeps a cone, and the widest that sum can lean is
	// asin(fuzz). That bound is what makes the parameter mean something:
	// it is why fuzz 1 is the widest lobe and why the reflection blurs by
	// a predictable amount rather than an arbitrary one.
	//
	// The second claim is the one worth having. The lobe has to be
	// CENTRED on the mirror direction, so the sideways components average
	// out. If the direction sampler ever falls back to a fixed vector --
	// which its rejection loop does, on a draw that runs out of attempts
	// -- the lobe leans that way, and nothing else in the suite would
	// notice: energy is untouched and the reflection still blurs.
	const glm::vec3 in = incident(30.0f);
	const glm::vec3 mirrorDirection = reflect(in, kNormal);
	const float fuzz = 0.3f;
	const MetalMaterial rough(glm::vec3(1.0f), fuzz);

	// Two directions across the lobe, to measure the lean in.
	const glm::vec3 sideways = glm::normalize(glm::cross(mirrorDirection, kNormal));
	const glm::vec3 upDownLobe = glm::cross(sideways, mirrorDirection);

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
		upDownMean.add((double) glm::dot(out, upDownLobe));
	}

	// A ray direction, so unit length.
	CHECK(worstLength < 1e-5f);

	// Inside the cone, on every draw and not merely on average.
	CHECK(worstLean >= widestLean - 1e-5f);

	// And centred in it.
	CHECK_ESTIMATE(sidewaysMean, 0.0);
	CHECK_ESTIMATE(upDownMean, 0.0);
}

TEST_CASE("metal: a perturbation into the surface is absorbed")
{
	// At a grazing angle a wide lobe hangs partly below the horizon. Those
	// draws cannot be scattered, so the material reports zero density and
	// zero weight, which is how the renderer is told the path ends here.
	// This is the one place a delta material returns a pdf of 0, and it is
	// deliberate: a rough metal is darker at its silhouette.
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
			// Absorbed means absorbed: no weight and no density, so the
			// renderer's `pdf <= 0` guard fires before it ever multiplies
			// the brdf into a throughput.
			CHECK(pdf == 0.0f);
			CHECK(brdf == glm::vec3(0.0f));
		} else {
			CHECK(pdf == 1.0f);
			CHECK(brdf == glm::vec3(1.0f));
		}
	}

	// The configuration has to actually reach the branch, or the assertions
	// above are checking nothing.
	CHECK(absorbed > 0);
	CHECK(absorbed < draws);
}

} // TEST_SUITE bsdf/metal
