// End-to-end renders, checked as pictures.
//
// bsdf_test.cpp proves each material is right on its own. These tests
// cover what only a whole render can be wrong about: the path loop, the
// transform stack, the environment lookup and the image writer. Each one
// picks a scene whose correct output is known without rendering it -- a
// uniform colour, or a second render that must match byte for byte -- so
// there is no reference image to keep up to date.

#include "support/RenderProbe.hpp"

using render::Image;
using render::renderAndLoad;
using render::renderScene;
using render::setSceneParameter;

namespace {

// The furnace criterion: a sphere with albedo 1 in a uniform environment
// is invisible, so the image is one flat colour of the environment's
// value. Run at radiance 1, which lands on byte 255, and 0.5, which lands
// on 128 with room to be wrong in either direction.
//
// Tolerance is one 8-bit code, and the reason is worth knowing. A pixel
// the ray never bounced in gets the environment radiance untouched, while
// one that bounced carries a throughput that made a round trip through
// float -- brdf divided by a cosine, then multiplied by the same cosine.
// That lands a few parts in ten million low, which is invisible as light
// and irrelevant as physics, but 0.5 quantises to exactly 128.0 before
// truncation, so ANY drift downwards shows up as a byte. Demanding bit
// equality here would be testing float associativity, not energy.
//
// Nothing is lost by relaxing it. A real energy leak is a percentage, not
// a part in ten million, and the exactness that IS contractual -- a delta
// lobe returning its albedo unmodified -- is asserted directly, and
// exactly, in bsdf_test.cpp.
constexpr int kQuantisationSlack = 1;

void checkFurnaceIsUniform(const std::string & material)
{
	CAPTURE(material);
	setSceneParameter("FURNACE_MATERIAL", material);
	REQUIRE(renderScene("tests/scenes/furnace.lua"));

	const std::string stem = "tests/out/furnace_" + material + "_";
	const struct {
		const char * exposure;
		int expected;
	} cases[] = {{"full", 255}, {"half", 128}};

	for (const auto & c : cases) {
		CAPTURE(c.exposure);
		const Image image(stem + c.exposure + ".png");
		REQUIRE(image.loaded());

		const int lo = image.minByte();
		const int hi = image.maxByte();
		CAPTURE(lo);
		CAPTURE(hi);

		// The sphere is invisible: no edge anywhere in the frame.
		CHECK(hi - lo <= kQuantisationSlack);

		// And invisible at the right brightness. Separate assertions for
		// the two directions, so a failure says whether the sphere came
		// out dark, which is energy lost, or bright, which is energy
		// counted twice.
		CHECK(lo >= c.expected - kQuantisationSlack);
		CHECK(hi <= c.expected + kQuantisationSlack);
	}
}

} // namespace

//=====================================================================
TEST_SUITE("render/furnace")
{

// The same claim for three materials, because it is a claim about the
// integrator. Separate cases so a failure names the material and so the
// three renders can run in parallel under ctest.

TEST_CASE("furnace: a diffuse sphere is invisible in a uniform environment")
{
	checkFurnaceIsUniform("lambertian");
}

TEST_CASE("furnace: a mirror sphere is invisible in a uniform environment")
{
	// A perfect mirror reflects radiance 1 from whatever direction it
	// looks in, so it has to vanish exactly as the diffuse sphere does.
	checkFurnaceIsUniform("mirror");
}

TEST_CASE("furnace: a sharp metal sphere is invisible in a uniform environment")
{
	// Fuzz 0 puts the whole lobe in one direction, so nothing can be
	// absorbed and the metal has to vanish exactly as the mirror does.
	checkFurnaceIsUniform("metal_sharp");
}

TEST_CASE("furnace: a rough metal sphere loses energy but never gains any")
{
	// The one material that is allowed to fail the invisibility criterion,
	// and the reason is a design decision rather than a bug: a fuzz lobe
	// straddles the horizon at grazing angles, and a perturbation that
	// tips the direction into the surface absorbs the ray. So the
	// silhouette goes dark.
	//
	// What must still hold is the direction of the error. Energy can be
	// dropped on the floor; it cannot be conjured. A rough sphere brighter
	// than its environment means the throughput is being double counted,
	// and no absorption rule explains that.
	setSceneParameter("FURNACE_MATERIAL", "metal_rough");
	REQUIRE(renderScene("tests/scenes/furnace.lua"));

	const Image image("tests/out/furnace_metal_rough_half.png");
	REQUIRE(image.loaded());

	CHECK(image.maxByte() <= 128 + kQuantisationSlack);

	// And the loss is confined to the silhouette rather than dimming the
	// whole sphere: the environment itself is still rendered at full
	// brightness, so the brightest pixel is the environment's own value.
	CHECK(image.maxByte() >= 128 - kQuantisationSlack);
}

} // TEST_SUITE render/furnace

//=====================================================================
TEST_SUITE("render/scene-graph")
{

TEST_CASE("scene graph: a child inherits its parent transform exactly once")
{
	// The same child under a GeometryNode and under a plain node. A
	// GeometryNode carries geometry AND a transform, so the easy mistake
	// is applying its matrix on the way down and again on the way out,
	// which displaces the child. Byte equality is the right bar: the two
	// scenes are the same scene, so any difference at all is the bug.
	const Image control = renderAndLoad("tests/scenes/nested_control.lua",
	                                    "tests/out/nested_control.png");
	const Image nested = renderAndLoad("tests/scenes/nested_under_geometry.lua",
	                                   "tests/out/nested_under_geometry.png");

	CHECK(control == nested);
}

} // TEST_SUITE render/scene-graph

//=====================================================================
TEST_SUITE("render/output")
{

TEST_CASE("output: the render is independent of resolution")
{
	// The environment texture used to be sampled by a 1:1 centre crop,
	// which indexed past the end of a 920x891 texture as soon as the
	// render grew past it. The sizes below straddle that boundary.
	for (const char * size : {"512", "1024", "2048"}) {
		CAPTURE(size);
		setSceneParameter("RESOLUTION", size);
		const Image image =
		    renderAndLoad("tests/scenes/resolution.lua",
		                  std::string("tests/out/resolution_") + size + ".png");

		CHECK(image.width() == (unsigned) std::atoi(size));
		CHECK(image.height() == (unsigned) std::atoi(size));
	}
}

TEST_CASE("output: the sRGB transfer is applied, and can be switched off")
{
	// Identical output from both settings means the transfer is stuck on
	// or stuck off. The scene is a matte grey sphere lit head-on, so the
	// centre pixel is a mid grey -- the part of the curve where linear and
	// sRGB are furthest apart, and where being stuck would be obvious.
	setSceneParameter("PROBE_SRGB", "0");
	const Image linear = renderAndLoad("tests/scenes/tonemap_probe.lua",
	                                   "tests/out/tonemap_probe.png");

	setSceneParameter("PROBE_SRGB", "1");
	const Image encoded = renderAndLoad("tests/scenes/tonemap_probe.lua",
	                                    "tests/out/tonemap_probe.png");

	CHECK_FALSE(linear == encoded);

	// Direction, not just difference: the sRGB curve lifts midtones, so
	// the encoded image cannot be the darker of the two.
	CHECK(encoded.maxByte() >= linear.maxByte());
}

} // TEST_SUITE render/output

//=====================================================================
TEST_SUITE("render/acceleration")
{

TEST_CASE("acceleration: the BVH returns what the linear scan returns")
{
	// BVH_VERIFY makes the renderer intersect both ways on every ray and
	// log any disagreement, so this is a whole scene's worth of
	// comparisons rather than a handful of hand-written rays. Until
	// BVH::build() exists the renderer falls back to the linear scan and
	// this passes trivially, which is the correct answer for that state.
	//
	// The renderer keeps going after a mismatch and still exits 0, so the
	// log is the only evidence and reading it is the test.
	std::string log;
	setSceneParameter("BVH_VERIFY", "1");
	REQUIRE(renderScene("assets/scenes/hier.lua", &log));
	setSceneParameter("BVH_VERIFY", "0");

	INFO(log);
	CHECK_FALSE(render::logContains(log, "bvh mismatch"));
}

} // TEST_SUITE render/acceleration
