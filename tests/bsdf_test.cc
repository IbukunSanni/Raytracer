// The materials, checked on their own -- no scene, no renderer, no image.
// Energy is the furnace's job (render_test.cc); these check what a furnace
// cannot see.
//
// Two claims:
//
//   1. SAMPLING. Sample() and Pdf() must describe the same distribution.
//      They are written separately and used together in a product where a
//      matched pair of errors cancels silently, so they are compared here
//      where nothing can cancel.
//
//   2. DIRECTION. A dielectric returns throughput 1 on both branches, so a
//      wrong direction or a wrong split between them still passes the
//      furnace. Snell, total internal reflection and Fresnel are checked
//      against values computed independently of this code.
//
//   3. EMISSION. A light adds its radiance through Emitted(), so it must
//      reflect nothing, and a material that only reflects must emit nothing.
//
//   4. PHASE. A medium scatters over the whole sphere, not a hemisphere, so
//      its weight must be the albedo in every direction, below the normal
//      included.

#include <string>

#include "scene/material.h"
#include "scene/scattering.h"
#include "support/bsdf_probe.h"

using probe::Incident;

//=====================================================================
TEST_SUITE("bsdf/lambertian") {
  TEST_CASE("lambertian: Sample() and Pdf() describe the same distribution") {
    const LambertianMaterial grey(glm::vec3(0.8f));
    probe::SamplerAgreement m = probe::MeasureSampler(grey, Incident(0.0f), 3u);

    CHECK_ESTIMATES_AGREE(m.pdf_mass, m.fraction_above);
    CHECK_ESTIMATE(m.cos_squared_integral, 2.0 * kPI / 3.0);
  }

}  // TEST_SUITE bsdf/lambertian

//=====================================================================
TEST_SUITE("bsdf/blinn-phong") {
  TEST_CASE("blinn-phong: Sample() and Pdf() agree at every incident angle") {
    // This lobe depends on where the light came from, so the pair has to
    // agree at every angle rather than only head-on. At 80 degrees most of
    // the lobe hangs below the horizon, which is where a sampler that
    // forgets to reject those draws parts company with its pdf.
    struct Case {
      std::string
          label;  // a std::string, so a failure prints it, not its address
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
    for (const Case& c : cases) {
      CAPTURE(c.label);
      const BlinnPhongMaterial m(glm::vec3(c.kd), glm::vec3(c.ks), c.exponent);
      probe::SamplerAgreement s =
          probe::MeasureSampler(m, Incident(c.degrees), seed++);

      CHECK_ESTIMATES_AGREE(s.pdf_mass, s.fraction_above);
      CHECK_ESTIMATE(s.cos_squared_integral, 2.0 * kPI / 3.0);
    }
  }

}  // TEST_SUITE bsdf/blinn-phong

//=====================================================================
// A dielectric's direction can be wrong while every energy measurement
// stays perfect: its throughput is 1 and the furnace is uniform, so each
// direction returns the same radiance. These check the direction itself.

namespace {

// A view direction `degrees` from the normal but on the far side of the
// surface, so the material is being left rather than entered.
glm::vec3 IncidentFromBelow(float degrees) {
  const glm::vec3 v = probe::Incident(degrees);
  return glm::vec3(v.x, v.y, -v.z);
}

struct Crossing {
  float ior;
  bool entering;
  float degrees;
};

// Every crossing worth naming, at both a denser medium and a thinner one.
// 1.5 is glass in air; 1/1.33 is air in water, where the critical angle is
// reached going IN rather than out -- the mirror image of the glass case.
const Crossing kCrossings[] = {
    {1.5f, true, 10.0f},          {1.5f, true, 41.0f},
    {1.5f, true, 50.0f},          {1.5f, true, 80.0f},
    {1.5f, false, 10.0f},         {1.5f, false, 30.0f},
    {1.5f, false, 40.0f},         {1.5f, false, 50.0f},
    {1.5f, false, 80.0f},

    {1.0f / 1.33f, true, 10.0f},  {1.0f / 1.33f, true, 40.0f},
    {1.0f / 1.33f, true, 50.0f},  {1.0f / 1.33f, true, 80.0f},
    {1.0f / 1.33f, false, 10.0f}, {1.0f / 1.33f, false, 80.0f},
};

// The ratio Snell is actually written in, for this direction of travel.
double RelativeIndex(const Crossing& c) {
  return c.entering ? 1.0 / static_cast<double>(c.ior)
                    : static_cast<double>(c.ior);
}

const glm::vec3 ViewFor(const Crossing& c) {
  return c.entering ? Incident(c.degrees) : IncidentFromBelow(c.degrees);
}

// The normal on the side the ray arrived from, so `dot(out, facing) > 0`
// means the ray went back the way it came.
glm::vec3 FacingFor(const Crossing& c) {
  return c.entering ? probe::kNormal : -probe::kNormal;
}

// Draws enough that the standard error on a fraction is a few parts in ten
// thousand, which is what lets the Fresnel tolerance below be tight.
constexpr int kDraws = 200000;

// Every draw at one crossing, split by which side it left through. Fresnel
// makes the branch a coin flip, so nothing here can be asserted on one draw.
struct Split {
  int reflected = 0;
  int transmitted = 0;
  double worst_snell_error = 0.0;   // over the transmitted draws
  float worst_mirror_error = 0.0f;  // over the reflected draws
};

Split DrawSplit(const Crossing& c, uint32_t seed) {
  const DielectricMaterial mat(c.ior);
  const glm::vec3 view_dir = ViewFor(c);
  const glm::vec3 facing = FacingFor(c);
  const glm::vec3 mirror = Reflect(view_dir, facing);
  const double eta = RelativeIndex(c);
  const double sin_in = std::sin(glm::radians(static_cast<double>(c.degrees)));

  Rng rng(seed);
  Split split;
  for (int i = 0; i < kDraws; ++i) {
    float pdf;
    glm::vec3 brdf;
    const glm::vec3 out = mat.Sample(rng, view_dir, probe::kHit, &pdf, &brdf);
    const double cos_out = glm::dot(out, facing);

    if (cos_out > 0.0) {
      ++split.reflected;
      split.worst_mirror_error =
          std::max(split.worst_mirror_error, glm::length(out - mirror));
    } else {
      ++split.transmitted;
      const double sin_out = std::sqrt(std::fmax(0.0, 1.0 - cos_out * cos_out));
      split.worst_snell_error =
          std::max(split.worst_snell_error, std::fabs(sin_out - eta * sin_in));
    }
  }
  return split;
}

}  // namespace

TEST_SUITE("bsdf/dielectric") {
  TEST_CASE(
      "dielectric: a transmitted ray obeys Snell on whichever draw it is") {
    // sin(theta_t) = eta * sin(theta_i), and the ray leaves through the far
    // side of the surface. A sign error in the tangential term keeps the
    // angle and flips the side, so both halves are asserted.
    //
    // Below the critical angle Fresnel still reflects a few percent of
    // draws, so this asserts over whichever ones transmitted rather than
    // over a single sample that used to be guaranteed to.
    uint32_t seed = 84u;
    for (const Crossing& c : kCrossings) {
      const double eta = RelativeIndex(c);
      const double sin_in =
          std::sin(glm::radians(static_cast<double>(c.degrees)));
      if (eta * sin_in > 1.0) continue;  // no transmitted branch to check

      CAPTURE(c.ior);
      CAPTURE(c.entering);
      CAPTURE(c.degrees);

      const Split split = DrawSplit(c, seed++);
      CHECK(split.transmitted > 0);  // or the bound below is vacuous
      CHECK(split.worst_snell_error < 1e-4);
    }
  }

  TEST_CASE(
      "dielectric: past the critical angle the ray reflects and only there") {
    // Total internal reflection is a property of the crossing, not of the
    // material: the same glass that can reflect a ray on the way out never
    // can on the way in. Testing the index in place of the ratio passes the
    // way out and fails the way in, which is why both appear here.
    //
    // "Only there" is now about certainty rather than direction: below the
    // critical angle some draws reflect, but every draw must reflect above
    // it, and each one has to land on the mirror direction exactly.
    int total_internal = 0;
    uint32_t seed = 85u;

    for (const Crossing& c : kCrossings) {
      CAPTURE(c.ior);
      CAPTURE(c.entering);
      CAPTURE(c.degrees);

      const double eta = RelativeIndex(c);
      const double sin_in =
          std::sin(glm::radians(static_cast<double>(c.degrees)));
      const bool expected = eta * sin_in > 1.0;

      const Split split = DrawSplit(c, seed++);
      if (expected) {
        ++total_internal;
        CHECK(split.transmitted == 0);
      } else {
        CHECK(split.transmitted > 0);
      }
      CHECK(split.worst_mirror_error < 1e-5f);
    }

    // Both branches have to be reachable, or the checks above are vacuous.
    CHECK(total_internal > 0);
    CHECK(total_internal <
          static_cast<int>(sizeof(kCrossings) / sizeof(kCrossings[0])));
  }

  TEST_CASE("dielectric: the reflected fraction is Fresnel's") {
    // The rung's real signal, and the only one that scores the split. The
    // furnace cannot: both branches carry throughput 1, so any mixture of
    // them returns radiance 1 in a uniform environment.
    //
    // Expected values are the dielectric Fresnel equations evaluated
    // independently, not this renderer's own Reflectance() -- a function
    // compared against itself would agree however wrong it was.
    struct Expected {
      float ior;
      bool entering;
      float degrees;
      double reflectance;
    };
    const Expected expected[] = {
        {1.5f, true, 0.0f, 0.040000},   {1.5f, true, 45.0f, 0.050240},
        {1.5f, true, 80.0f, 0.387704},  {1.5f, false, 0.0f, 0.040000},
        {1.5f, false, 30.0f, 0.055190}, {1.5f, false, 40.0f, 0.245291},
        {1.0f, true, 80.0f, 0.000000},
    };

    uint32_t seed = 90u;
    for (const Expected& e : expected) {
      CAPTURE(e.ior);
      CAPTURE(e.entering);
      CAPTURE(e.degrees);

      const Crossing c{e.ior, e.entering, e.degrees};
      const Split split = DrawSplit(c, seed++);
      const double measured =
          static_cast<double>(split.reflected) / static_cast<double>(kDraws);

      // Four standard errors at the widest split in the table, which is
      // loose enough never to flake and tight enough to reject Schlick --
      // it misses the 40-degree row by 0.204.
      CHECK(std::fabs(measured - e.reflectance) < 0.005);
    }
  }

}  // TEST_SUITE bsdf/dielectric

//=====================================================================
TEST_SUITE("bsdf/diffuse-light") {
  TEST_CASE("diffuse light: emits its texture and reflects nothing") {
    const glm::vec3 radiance(4.0f, 2.0f, 1.0f);  // above 1, as lights are
    const DiffuseLight lamp(radiance);
    CHECK(lamp.Emitted(probe::kHit) == radiance);

    // The zero pdf is what ends the path at the light.
    Rng rng(5u);
    float pdf = 1.0f;
    glm::vec3 brdf(1.0f);
    lamp.Sample(rng, Incident(30.0f), probe::kHit, &pdf, &brdf);
    CHECK(pdf == 0.0f);
    CHECK(brdf == glm::vec3(0.0f));
    CHECK(lamp.Eval(Incident(30.0f), probe::kHit, probe::kNormal) ==
          glm::vec3(0.0f));
  }

  TEST_CASE("diffuse light: a material that only reflects emits nothing") {
    const LambertianMaterial grey(glm::vec3(0.8f));
    const DielectricMaterial glass(1.5f);
    CHECK(grey.Emitted(probe::kHit) == glm::vec3(0.0f));
    CHECK(glass.Emitted(probe::kHit) == glm::vec3(0.0f));
  }

}  // TEST_SUITE bsdf/diffuse-light

//=====================================================================
TEST_SUITE("bsdf/volume-scatter") {
  TEST_CASE("volume scatter: Eval() over Pdf() is the albedo in every direction") {
    const glm::vec3 albedo(0.9f, 0.5f, 0.2f);
    const VolumeScatter smoke(albedo);
    CHECK_FALSE(smoke.HasSurface());

    // Below the normal included: a medium has no horizon.
    for (float degrees : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f}) {
      CAPTURE(degrees);
      const glm::vec3 out = Incident(degrees);
      const glm::vec3 ratio = smoke.Eval(Incident(30.0f), probe::kHit, out) /
                              smoke.Pdf(Incident(30.0f), probe::kHit, out);
      CHECK(glm::all(glm::lessThan(glm::abs(ratio - albedo),
                                   glm::vec3(1e-6f))));
    }

    // And what Sample() writes agrees with both.
    const probe::Draw d = probe::DrawOnce(smoke, Incident(30.0f), 9u);
    CHECK(d.pdf == smoke.Pdf(Incident(30.0f), probe::kHit, d.direction));
    CHECK(d.brdf == smoke.Eval(Incident(30.0f), probe::kHit, d.direction));
  }

  TEST_CASE("volume scatter: Sample() is uniform over the sphere") {
    const VolumeScatter smoke(glm::vec3(0.5f));
    probe::SamplerAgreement m =
        probe::MeasureSampler(smoke, Incident(30.0f), 11u);

    // Half the mass above the normal, from both ends.
    CHECK_ESTIMATES_AGREE(m.pdf_mass, m.fraction_above);
    CHECK_ESTIMATE(m.fraction_above, 0.5);
    // cos^2 over the whole sphere is 4*pi/3, twice the hemisphere's.
    CHECK_ESTIMATE(m.cos_squared_integral, 4.0 * kPI / 3.0);

    // No preferred direction: the mean of the draws is the origin. A
    // hemisphere sampler puts z at 1/2, a cosine-weighted one at 2/3.
    Rng rng(13u);
    stats::Estimate x, y, z;
    for (int i = 0; i < probe::kSamples; ++i) {
      float pdf = 0.0f;
      glm::vec3 brdf(0.0f);
      const glm::vec3 w =
          smoke.Sample(rng, Incident(30.0f), probe::kHit, &pdf, &brdf);
      x.Add(w.x);
      y.Add(w.y);
      z.Add(w.z);
    }
    CHECK_ESTIMATE(x, 0.0);
    CHECK_ESTIMATE(y, 0.0);
    CHECK_ESTIMATE(z, 0.0);
  }

}  // TEST_SUITE bsdf/volume-scatter
