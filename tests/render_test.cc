// Whole renders, checked as pictures.
//
// These cover what only a full render can be wrong about: the path loop,
// the transform stack, the environment lookup and the image writer.
//
// Every scene here has an output you can predict without rendering it,
// either one flat colour or a second render that must match byte for byte.
// So there are no reference images to store, regenerate, or argue with.

#include <cmath>

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
//
// BSDF sampling only. With the mixture a path's weight is no longer exactly
// 1, so a lamp's frame is flat only on average; that claim has its own test.
void CheckFurnaceIsUniform(const std::string& material, bool moving = false,
                           bool lamp = false) {
  CAPTURE(material);
  CAPTURE(moving);
  CAPTURE(lamp);
  SetSceneParameter("FURNACE_MATERIAL", material);
  SetSceneParameter("FURNACE_MOTION", moving ? "1" : "0");
  SetSceneParameter("FURNACE_LAMP", lamp ? "1" : "0");
  SetSceneParameter("RT_MIXTURE", "0");
  REQUIRE(RenderScene("tests/scenes/furnace.lua"));
  const std::string tag =
      material + (moving ? "_moving" : "") + (lamp ? "_lamp" : "");

  const struct {
    const char* exposure;
    int expected;
  } passes[] = {{"full", 255}, {"half", 128}};

  for (const auto& pass : passes) {
    CAPTURE(pass.exposure);
    const Image image("tests/out/furnace_" + tag + "_" + pass.exposure +
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
  // One claim, three materials, because it is a claim about the integrator
  // rather than about any one of them. Separate cases so a failure names the
  // material, and so the renders run in parallel.

  TEST_CASE("furnace: a diffuse sphere is invisible in a uniform environment") {
    CheckFurnaceIsUniform("lambertian");
  }

  TEST_CASE("furnace: a lamp as bright as the sky is invisible too") {
    // The lamp the mixture aims at, checked first without the mixture. A
    // face that showed its dark back, or a gap between faces, would leave
    // an edge here, and the mixture's test could not tell it from noise.
    CheckFurnaceIsUniform("lambertian", /*moving=*/false, /*lamp=*/true);
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

  TEST_CASE(
      "furnace: a glass sphere neither loses nor gains energy on average") {
    // A real interface, where the eta^2 radiance scaling is not 1, and so
    // the one furnace material that is unbiased but noisy: entering drops
    // the throughput below Russian roulette's clamp, killing more paths
    // inside the glass and boosting the survivors to compensate.
    //
    // Hence the mean rather than min and max. At 64 spp the frame spans
    // 108 to 141 around an expected 128, so the invisibility criterion the
    // other materials meet would be measuring roulette, not the BSDF.
    //
    // This catches a gross energy error. It cannot catch the sign of the
    // exponent, which the bsdf suite scores directly: entering and leaving
    // carry reciprocals, so a complete path returns 1 for either sign.
    SetSceneParameter("FURNACE_MATERIAL", "dielectric_glass");
    REQUIRE(RenderScene("tests/scenes/furnace.lua"));

    const Image image("tests/out/furnace_dielectric_glass_half.png");
    REQUIRE(image.Loaded());

    // A byte at 64 spp. Loose against the noise above, and far tighter
    // than any real leak: dropping the leaving factor alone, so that
    // entering scales and exiting does not, measures 98.5 here.
    CAPTURE(image.MeanByte());
    CHECK(std::fabs(image.MeanByte() - 128.0) < 1.0);
  }

  TEST_CASE("furnace: an albedo-1 medium neither loses nor gains energy") {
    // Smoke scatters several times per path, so roulette makes it noisy
    // like glass, and it gets the same test. The ball's unbiased mean is
    // 0.5 * 255 = 127.5: only the noiseless surround rounds that tie up to
    // 128, which puts the frame near 127.8.
    //
    // A cosine left in the scattering weight measures 101.3 here.
    SetSceneParameter("FURNACE_MATERIAL", "volume");
    REQUIRE(RenderScene("tests/scenes/furnace.lua"));

    const Image image("tests/out/furnace_volume_half.png");
    REQUIRE(image.Loaded());

    CAPTURE(image.MeanByte());
    CHECK(std::fabs(image.MeanByte() - 128.0) < 1.0);
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
TEST_SUITE("render/emission") {
  TEST_CASE("emission: a light shows its radiance and nothing it is lit by") {
    const Image image = RenderAndLoad("tests/scenes/diffuse_light.lua",
                                      "tests/out/diffuse_light.png");
    const int lo = image.MinByte();
    const int hi = image.MaxByte();
    CAPTURE(lo);
    CAPTURE(hi);

    // The black environment stays black around the light.
    CHECK(lo == 0);

    // Dark means it is not emitting; bright means the point light aimed at
    // it was reflected, which a light must not do.
    CHECK(hi >= 128 - kQuantisationSlack);
    CHECK(hi <= 128 + kQuantisationSlack);
  }

}  // TEST_SUITE render/emission

//=====================================================================
TEST_SUITE("render/light-sampling") {
  TEST_CASE("light sampling: the mixture lights what bouncing lights") {
    // Only direct light exists in this scene, so the two ways of tracing it
    // are two estimates of one integral. A wrong density, a lamp in the wrong
    // place, or a shadow ignored moves one mean and not the other.
    const std::string scene = "tests/scenes/light_sampling.lua";
    const std::string output = "tests/out/light_sampling.png";

    SetSceneParameter("RT_MIXTURE", "0");
    const Image bounced = RenderAndLoad(scene, output);
    SetSceneParameter("RT_MIXTURE", "1");
    const Image mixed = RenderAndLoad(scene, output);
    SetSceneParameter("RT_MIXTURE", "0");

    const double bounced_mean = bounced.MeanByte();
    const double mixed_mean = mixed.MeanByte();
    CAPTURE(bounced_mean);
    CAPTURE(mixed_mean);

    // Different paths, or the switch did nothing and the means agree trivially.
    CHECK_FALSE(mixed == bounced);

    // Lit well above black and short of clipping, so the means can differ.
    CHECK(bounced_mean > 40.0);
    CHECK(bounced.MaxByte() < 255);
    CHECK(mixed.MaxByte() < 255);

    // A percent: well above the noise of about a million paths, and well
    // below any missing factor.
    CHECK(mixed_mean == doctest::Approx(bounced_mean).epsilon(0.01));
  }

  TEST_CASE("light sampling: a sphere lamp is aimed at, and lights the same") {
    // The far lamp is a ball, so the shapes aimed at are a quad and a sphere:
    // a sphere's cone density wrong by any factor, or a picked index that
    // lands on the wrong list, moves the mixture's mean and not bouncing's.
    const std::string scene = "tests/scenes/light_sampling.lua";
    const std::string output = "tests/out/light_sampling_sphere.png";

    SetSceneParameter("LIGHT_SAMPLING_LAMP", "sphere");
    SetSceneParameter("RT_MIXTURE", "0");
    const Image bounced = RenderAndLoad(scene, output);
    SetSceneParameter("RT_MIXTURE", "1");
    const Image mixed = RenderAndLoad(scene, output);
    SetSceneParameter("RT_MIXTURE", "0");
    SetSceneParameter("LIGHT_SAMPLING_LAMP", "quad");

    const double bounced_mean = bounced.MeanByte();
    const double mixed_mean = mixed.MeanByte();
    CAPTURE(bounced_mean);
    CAPTURE(mixed_mean);

    CHECK_FALSE(mixed == bounced);
    CHECK(bounced_mean > 40.0);
    CHECK(bounced.MaxByte() < 255);
    CHECK(mixed.MaxByte() < 255);

    // A percent, as above. At 256 spp the bounced frame sits about half a
    // percent high: truncating to a byte keeps the rare bright sample in a
    // dark, noisy pixel and rounds a smooth one down. At 4096 spp the two
    // agree within 0.05%, so the gap is the quantising, not the sampler.
    CHECK(mixed_mean == doctest::Approx(bounced_mean).epsilon(0.01));
  }

  TEST_CASE("light sampling: aiming at glass moves the noise, not the mean") {
    // The book aims at its glass ball though it gives off nothing. That is
    // only safe because the density divided by is the one the direction was
    // drawn from: leave the ball out of that density and every direction
    // aimed at it counts as if it were rarer than it was.
    const std::string scene = "tests/scenes/light_sampling.lua";

    SetSceneParameter("RT_MIXTURE", "1");
    SetSceneParameter("LIGHT_SAMPLING_BALL", "glass");
    const Image plain =
        RenderAndLoad(scene, "tests/out/light_sampling_glass.png");
    SetSceneParameter("LIGHT_SAMPLING_BALL", "glass_sampled");
    const Image aimed =
        RenderAndLoad(scene, "tests/out/light_sampling_glass_sampled.png");
    SetSceneParameter("LIGHT_SAMPLING_BALL", "black");
    SetSceneParameter("RT_MIXTURE", "0");

    const double plain_mean = plain.MeanByte();
    const double aimed_mean = aimed.MeanByte();
    CAPTURE(plain_mean);
    CAPTURE(aimed_mean);

    // Different paths, or the mark did nothing and the means agree trivially.
    CHECK_FALSE(aimed == plain);

    // The caustic makes both frames noisier than the black ball's: across
    // seeds each mean wanders by about 0.2%.
    CHECK(aimed_mean == doctest::Approx(plain_mean).epsilon(0.01));
  }

  TEST_CASE("light sampling: the mixture keeps a lit furnace at the sky") {
    // The furnace with its lamp, now with half of each bounce aimed at it.
    // Every direction still brings back the sky's radiance, so the mean must
    // stay there; a density that drops either half, or counts one twice, moves
    // it by roughly the share of the ball's sky the lamp fills.
    //
    // The half-exposure frame only. A bounce's weight is the material's
    // density over the mixture's, which is at most 2, and the ball is convex
    // so a path bounces off it once: nothing passes radiance 1, so nothing
    // clips and the mean is not dragged down.
    SetSceneParameter("FURNACE_MATERIAL", "lambertian");
    SetSceneParameter("FURNACE_MOTION", "0");
    SetSceneParameter("FURNACE_LAMP", "1");
    SetSceneParameter("RT_MIXTURE", "1");
    const bool rendered = RenderScene("tests/scenes/furnace.lua");
    SetSceneParameter("RT_MIXTURE", "0");
    SetSceneParameter("FURNACE_LAMP", "0");
    REQUIRE(rendered);

    const Image image("tests/out/furnace_lambertian_lamp_mixture_half.png");
    REQUIRE(image.Loaded());
    const double mean = image.MeanByte();
    CAPTURE(mean);
    CAPTURE(image.MinByte());
    CAPTURE(image.MaxByte());

    // Noise on the ball shows the mixture ran; an exactly flat frame means
    // the lamps were never found and this was BSDF sampling again.
    CHECK(image.MinByte() < image.MaxByte());

    // A percent, as above. Truncating a noisy pixel to a byte loses half a
    // byte on average, which costs the frame a fraction of one, well inside.
    CHECK(mean == doctest::Approx(128.0).epsilon(0.01));
  }

}  // TEST_SUITE render/light-sampling

//=====================================================================
TEST_SUITE("render/motion") {
  TEST_CASE("motion: a moving sphere still vanishes in the furnace") {
    // Motion blur decides where a sample meets the sphere, never how much
    // light comes back, so it cannot be a source or a sink of energy.
    CheckFurnaceIsUniform("lambertian", /*moving=*/true);
  }

  TEST_CASE("motion: the sphere moves as far as the scene says, no further") {
    // The pixels covered for the whole shutter are where the sphere's
    // first and last positions overlap: a lens, whose area is predicted
    // here from the scene's geometry before looking at the render. Coverage
    // alone cannot see speed -- a faster sphere still covers the same area
    // at each instant -- but the lens shrinks as the sphere moves further,
    // to nothing at twice this distance.
    //
    // tests/scenes/motion_coverage.lua: radius 60 at depth 500, centre
    // moving from x = -30 to x = 30, 128 x 128 pixels, 30 degree field.
    const double pixels_per_unit_slope =
        64.0 / std::tan(15.0 * std::acos(-1.0) / 180.0);  // half the field
    const double r = std::tan(std::asin(60.0 / 500.0)) * pixels_per_unit_slope;
    const double d = (60.0 / 500.0) * pixels_per_unit_slope;
    const double lens = 2.0 * r * r * std::acos(d / (2.0 * r)) -
                        0.5 * d * std::sqrt(4.0 * r * r - d * d);

    SetSceneParameter("MOTION_TAG", "travel");
    REQUIRE(RenderScene("tests/scenes/motion_coverage.lua"));
    const Image moving("tests/out/motion_travel_moving.png");
    REQUIRE(moving.Loaded());
    // Black on white, grey throughout: three equal bytes per pixel.
    const double always_covered = moving.CountBetween(-1, 1) / 3.0;
    CAPTURE(lens);
    CAPTURE(always_covered);
    CHECK(std::fabs(always_covered - lens) < 0.1 * lens);
  }

}  // TEST_SUITE render/motion
