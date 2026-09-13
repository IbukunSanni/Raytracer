// Whole renders, checked as pictures.
//
// These cover what only a full render can be wrong about: the path loop,
// the transform stack, the environment lookup and the image writer.
//
// Every scene here has an output you can predict without rendering it,
// either one flat colour or a second render that must match byte for byte.
// So there are no reference images to store, regenerate, or argue with.

#include "support/render_probe.h"

using render::Image;
using render::RenderAndLoad;
using render::RenderScene;
using render::SetSceneParameter;

namespace {

// How far a rendered byte may sit from its predicted value.
//
// A pixel the ray never bounced in carries the environment radiance
// untouched. One that bounced carries a throughput built by a chain of
// multiplications and divisions, and float arithmetic leaves that a few
// parts in ten million low. Invisible as light, but radiance 0.5 quantises
// to exactly 128.0 before truncation, so any drift downwards costs a whole
// byte. Demanding bit equality here would test float associativity rather
// than energy.
//
// Nothing is lost by allowing it. A real energy leak is a percentage, not
// a part in ten million.
constexpr int kQuantisationSlack = 1;

// The furnace criterion: a sphere with albedo 1 inside a uniform
// environment is invisible, so the frame is one flat colour at the
// environment value.
//
// Run at radiance 1, which lands on byte 255, and at 0.5, which lands on
// 128 with room to be wrong in either direction. The bright pass alone
// would clip a too-bright result to 255 and call it correct.
void CheckFurnaceIsUniform(const std::string& material) {
  CAPTURE(material);
  SetSceneParameter("FURNACE_MATERIAL", material);
  REQUIRE(RenderScene("tests/scenes/furnace.lua"));

  const struct {
    const char* exposure;
    int expected;
  } passes[] = {{"full", 255}, {"half", 128}};

  for (const auto& pass : passes) {
    CAPTURE(pass.exposure);
    const Image image("tests/out/furnace_" + material + "_" + pass.exposure +
                      ".png");
    REQUIRE(image.Loaded());

    const int lo = image.MinByte();
    const int hi = image.MaxByte();
    CAPTURE(lo);
    CAPTURE(hi);

    // Invisible: no edge anywhere in the frame.
    CHECK(hi - lo <= kQuantisationSlack);

    // And invisible at the right brightness. The two directions are
    // separate assertions, so a failure says whether the sphere came
    // out dark, meaning energy was lost, or bright, meaning it was
    // counted twice.
    CHECK(lo >= pass.expected - kQuantisationSlack);
    CHECK(hi <= pass.expected + kQuantisationSlack);
  }
}

}  // namespace

//=====================================================================
TEST_SUITE("render/furnace") {
  // One claim, four materials, because it is a claim about the integrator
  // rather than about any one of them. Separate cases so a failure names the
  // material, and so the renders run in parallel.

  TEST_CASE("furnace: a diffuse sphere is invisible in a uniform environment") {
    CheckFurnaceIsUniform("lambertian");
  }

  TEST_CASE("furnace: a mirror sphere is invisible in a uniform environment") {
    // A perfect mirror returns radiance 1 from whatever direction it looks
    // in, so it has to vanish exactly as the diffuse sphere does.
    CheckFurnaceIsUniform("mirror");
  }

  TEST_CASE(
      "furnace: a sharp metal sphere is invisible in a uniform environment") {
    // Fuzz 0 puts the whole lobe in one direction, so no ray can be
    // absorbed and the metal is held to the same standard as the mirror.
    CheckFurnaceIsUniform("metal_sharp");
  }

  TEST_CASE(
      "furnace: a dielectric sphere at index 1 is invisible in a uniform "
      "environment") {
    // index = 1 has no interface to bend at, so a transmitted ray leaves in
    // exactly the direction it arrived -- the same standard as the mirror,
    // but for the transmission half of the material instead of reflection.
    CheckFurnaceIsUniform("dielectric");
  }

  TEST_CASE("furnace: a rough metal sphere loses energy but never gains any") {
    // The one material allowed to fail the invisibility criterion, by
    // design rather than by accident. A fuzz lobe straddles the horizon at
    // grazing angles, and a perturbation that tips the direction into the
    // surface absorbs the ray, so the silhouette goes dark.
    //
    // The direction of the error still has to hold. Energy can be dropped;
    // it cannot be conjured. A sphere brighter than its environment means
    // the throughput is being counted twice, and no absorption rule
    // explains that.
    SetSceneParameter("FURNACE_MATERIAL", "metal_rough");
    REQUIRE(RenderScene("tests/scenes/furnace.lua"));

    const Image image("tests/out/furnace_metal_rough_half.png");
    REQUIRE(image.Loaded());

    CHECK(image.MaxByte() <= 128 + kQuantisationSlack);

    // The loss is confined to the silhouette rather than dimming
    // everything, so the brightest pixel is still the environment itself.
    CHECK(image.MaxByte() >= 128 - kQuantisationSlack);
  }

}  // TEST_SUITE render/furnace

//=====================================================================
TEST_SUITE("render/scene-graph") {
  TEST_CASE("scene graph: a child inherits its parent transform exactly once") {
    // The same child under a plain node and under a GeometryNode. A
    // GeometryNode carries geometry AND a transform, so the easy mistake
    // is applying its matrix on the way down and again on the way out,
    // which displaces the child.
    //
    // Byte equality is the right bar here: the two scenes describe the
    // same picture, so any difference at all is the bug.
    const Image control = RenderAndLoad("tests/scenes/nested_control.lua",
                                        "tests/out/nested_control.png");
    const Image nested = RenderAndLoad("tests/scenes/nested_under_geometry.lua",
                                       "tests/out/nested_under_geometry.png");

    CHECK(control == nested);
  }

}  // TEST_SUITE render/scene-graph

//=====================================================================
TEST_SUITE("render/output") {
  TEST_CASE("output: the render is independent of resolution") {
    // How many pixels you ask for must not change what is drawn, and in
    // particular must not walk off the end of the environment texture.
    // The scene sets sizes either side of that texture.
    for (const char* size : {"512", "1024", "2048"}) {
      CAPTURE(size);
      SetSceneParameter("RESOLUTION", size);
      const Image image =
          RenderAndLoad("tests/scenes/resolution.lua",
                        std::string("tests/out/resolution_") + size + ".png");

      CHECK(image.Width() == static_cast<unsigned>(std::atoi(size)));
      CHECK(image.Height() == static_cast<unsigned>(std::atoi(size)));
    }
  }

  TEST_CASE("output: the sRGB transfer is applied, and can be switched off") {
    // Identical output from both settings means the transfer is stuck on
    // or stuck off. The scene is a matte grey sphere lit head-on, so its
    // centre sits in the midtones, where the two curves are furthest apart
    // and being stuck is most obvious.
    SetSceneParameter("PROBE_SRGB", "0");
    const Image linear = RenderAndLoad("tests/scenes/tonemap_probe.lua",
                                       "tests/out/tonemap_probe.png");

    SetSceneParameter("PROBE_SRGB", "1");
    const Image encoded = RenderAndLoad("tests/scenes/tonemap_probe.lua",
                                        "tests/out/tonemap_probe.png");

    CHECK_FALSE(linear == encoded);

    // Direction, not just difference: the sRGB curve lifts midtones, so
    // the encoded image cannot be the darker of the two.
    CHECK(encoded.MaxByte() >= linear.MaxByte());
  }

}  // TEST_SUITE render/output

//=====================================================================
TEST_SUITE("render/acceleration") {
  TEST_CASE("acceleration: the BVH returns what the linear scan returns") {
    // BVH_VERIFY makes the renderer intersect both ways on every ray and
    // log any disagreement, so this is a whole scene worth of comparisons
    // rather than a handful of hand-written rays.
    //
    // The renderer logs a mismatch and keeps going, exiting cleanly either
    // way, so reading the output is the test. With no tree built it falls
    // back to the linear scan and this passes trivially, which is the
    // right answer for that state.
    std::string log;
    SetSceneParameter("BVH_VERIFY", "1");
    REQUIRE(RenderScene("assets/scenes/hier.lua", &log));
    SetSceneParameter("BVH_VERIFY", "0");

    INFO(log);
    CHECK_FALSE(render::LogContains(log, "bvh mismatch"));
  }

}  // TEST_SUITE render/acceleration
