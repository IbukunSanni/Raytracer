// The materials, checked on their own -- no scene, no renderer, no image.
//
// Three claims, applied to each material in turn.
//
//   1. ENERGY. A surface cannot return more light than it received. Its
//      directional albedo is at most 1, and exactly 1 for a white diffuse,
//      the one case with an answer in closed form.
//
//   2. SAMPLING. Sample() and Pdf() must describe the same distribution.
//      They are written separately and used together in a product where a
//      matched pair of errors cancels silently, so they are compared here
//      where nothing can cancel.
//
//   3. THE DELTA CONTRACT. A mirror scatters into a single direction, so
//      it carries no density. Eval() and Pdf() are zero, Sample() reports
//      a pdf of 1, and the weight it hands back is applied as-is by
//      whoever called it -- which is why that weight must be exact rather
//      than merely close.

#include "scene/material.h"
#include "scene/scattering.h"
#include "support/bsdf_probe.h"

using probe::Incident;
using probe::kNormal;

//=====================================================================
TEST_SUITE("bsdf/lambertian") {
  TEST_CASE("lambertian: albedo 1 returns every photon it receives") {
    const LambertianMaterial white(glm::vec3(1.0f));
    stats::Estimate3 rho = probe::DirectionalAlbedo(white, Incident(0.0f), 1u);
    CHECK_ESTIMATE3(rho, glm::vec3(1.0f));
  }

  TEST_CASE("lambertian: albedo is reproduced per channel at a grazing angle") {
    // A diffuse lobe is constant in every direction, so the angle of
    // incidence must not change what comes back.
    const glm::vec3 albedo(0.9f, 0.5f, 0.2f);
    const LambertianMaterial coloured(albedo);
    stats::Estimate3 rho =
        probe::DirectionalAlbedo(coloured, Incident(60.0f), 4u);
    CHECK_ESTIMATE3(rho, albedo);
  }

  TEST_CASE("lambertian: the sampler really is cosine distributed") {
    // A uniform disk sample lifted to the hemisphere is cosine
    // distributed, so E[cos] = integral of cos^2/pi dw = 2/3.
    const LambertianMaterial white(glm::vec3(1.0f));
    Rng rng(2u);
    stats::Estimate mean_cosine;

    for (int i = 0; i < probe::kSamples; ++i) {
      float pdf;
      glm::vec3 brdf;
      const glm::vec3 out =
          white.Sample(rng, Incident(0.0f), kNormal, &pdf, &brdf);
      mean_cosine.Add(static_cast<double>(glm::dot(kNormal, out)));
    }
    CHECK_ESTIMATE(mean_cosine, 2.0 / 3.0);
  }

  TEST_CASE("lambertian: Sample() and Pdf() describe the same distribution") {
    const LambertianMaterial grey(glm::vec3(0.8f));
    probe::SamplerAgreement m = probe::MeasureSampler(grey, Incident(0.0f), 3u);

    CHECK_ESTIMATES_AGREE(m.pdf_mass, m.fraction_above);
    CHECK_ESTIMATE(m.cos_squared_integral, 2.0 * kPI / 3.0);
  }

}  // TEST_SUITE bsdf/lambertian

//=====================================================================
TEST_SUITE("bsdf/blinn-phong") {
  TEST_CASE("blinn-phong: kd + ks <= 1 conserves energy at every angle") {
    // An un-normalised specular lobe fails this, brightly. The exponent
    // sets how wide the lobe is and the normalisation factor has to track
    // it, so the cases sweep both the split between the two lobes and how
    // tight the specular one is.
    struct Case {
      float kd, ks;
      double exponent;
    };
    const Case cases[] = {
        {0.6f, 0.3f, 1.0},    // the widest specular lobe
        {0.6f, 0.3f, 25.0},   // a typical highlight
        {0.2f, 0.7f, 200.0},  // specular-dominant and tight
        {0.0f, 0.9f, 50.0},   // specular only
        {0.9f, 0.0f, 50.0},   // diffuse only
    };
    const float angles[] = {0.0f, 45.0f, 80.0f};

    uint32_t seed = 10u;
    for (const Case& c : cases) {
      const BlinnPhongMaterial m(glm::vec3(c.kd), glm::vec3(c.ks), c.exponent);
      for (float degrees : angles) {
        CAPTURE(c.kd);
        CAPTURE(c.ks);
        CAPTURE(c.exponent);
        CAPTURE(degrees);
        stats::Estimate3 rho =
            probe::DirectionalAlbedo(m, Incident(degrees), seed++);
        CHECK_ESTIMATE_AT_MOST(rho.channel[0], 1.0);
      }
    }
  }

  TEST_CASE("blinn-phong: Sample() and Pdf() agree at every incident angle") {
    // This lobe depends on where the light came from, so the pair has to
    // agree at every angle rather than only head-on. At 80 degrees most of
    // the lobe hangs below the horizon, which is where a sampler that
    // forgets to reject those draws parts company with its pdf.
    struct Case {
      const char* label;
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
// Mirror and metal are the same delta lobe, so they answer to the same
// contract and share the helper that states it.

namespace {

void CheckDeltaContract(const Material& mat, const glm::vec3& view_dir,
                        const glm::vec3& expected_albedo, uint32_t seed) {
  const probe::Draw d = probe::DrawOnce(mat, view_dir, seed);

  CHECK(mat.IsSpecular());
  CHECK(d.pdf == 1.0f);

  // Exact, not approximate: the caller multiplies this weight straight
  // into a running product, so drift here is drift on every bounce.
  CHECK(d.brdf.r == expected_albedo.r);
  CHECK(d.brdf.g == expected_albedo.g);
  CHECK(d.brdf.b == expected_albedo.b);

  // No density to report, asked from either end.
  CHECK(mat.Eval(view_dir, kNormal, d.direction) == glm::vec3(0.0f));
  CHECK(mat.Pdf(view_dir, kNormal, d.direction) == 0.0f);
}

}  // namespace

TEST_SUITE("bsdf/mirror") {
  TEST_CASE("mirror: the delta contract holds at any angle and any albedo") {
    const MirrorMaterial white(glm::vec3(1.0f));
    CheckDeltaContract(white, Incident(0.0f), glm::vec3(1.0f), 50u);
    CheckDeltaContract(white, Incident(60.0f), glm::vec3(1.0f), 51u);

    const glm::vec3 tint(0.9f, 0.5f, 0.2f);
    CheckDeltaContract(MirrorMaterial(tint), Incident(0.0f), tint, 52u);
  }

  TEST_CASE("mirror: the scattered direction is the reflected direction") {
    const MirrorMaterial white(glm::vec3(1.0f));
    const glm::vec3 view_dir = Incident(60.0f);
    CHECK(probe::DrawOnce(white, view_dir, 53u).direction ==
          Reflect(view_dir, kNormal));
  }

}  // TEST_SUITE bsdf/mirror

//=====================================================================
TEST_SUITE("bsdf/metal") {
  // Metal perturbs the mirror direction, and absorbs the ray if the
  // perturbation tipped it into the surface. Absorption is only reachable
  // where the lobe straddles the horizon, so the cases below keep the two
  // regimes apart: head-on, where nothing can be absorbed and the delta
  // contract must hold on every draw, and grazing, where absorption is the
  // behaviour under test.

  TEST_CASE("metal: a scattered ray carries exactly the albedo, at any fuzz") {
    // Head-on, so the whole lobe stays above the surface however wide it
    // is. Roughening the reflection must not change what the rays carry.
    const glm::vec3 tint(0.9f, 0.5f, 0.2f);
    const float radii[] = {0.0f, 0.4f, 0.9f};

    uint32_t seed = 60u;
    for (float fuzz : radii) {
      CAPTURE(fuzz);
      CheckDeltaContract(MetalMaterial(tint, fuzz), Incident(0.0f), tint,
                         seed++);
    }
  }

  TEST_CASE("metal: fuzz 0 is a mirror, exactly") {
    // The degenerate case has to collapse onto the material it
    // generalises, or the two are separate implementations of one physics.
    const glm::vec3 view_dir = Incident(60.0f);
    const MetalMaterial sharp(glm::vec3(1.0f), 0.0f);
    CHECK(probe::DrawOnce(sharp, view_dir, 62u).direction ==
          Reflect(view_dir, kNormal));
  }

  TEST_CASE(
      "metal: the lobe is a cone of half-angle asin(fuzz) about the mirror") {
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
    const glm::vec3 view_dir = Incident(30.0f);
    const glm::vec3 mirror_direction = Reflect(view_dir, kNormal);
    const float fuzz = 0.3f;
    const MetalMaterial rough(glm::vec3(1.0f), fuzz);

    // Two directions across the lobe, to measure any lean in.
    const glm::vec3 sideways =
        glm::normalize(glm::cross(mirror_direction, kNormal));
    const glm::vec3 up_down = glm::cross(sideways, mirror_direction);

    const float widest_lean = std::sqrt(1.0f - fuzz * fuzz);  // cos(asin(fuzz))

    Rng rng(63u);
    stats::Estimate sideways_mean, up_down_mean;
    float worst_length = 0.0f;
    float worst_lean = 1.0f;

    for (int i = 0; i < 200000; ++i) {
      float pdf;
      glm::vec3 brdf;
      const glm::vec3 out = rough.Sample(rng, view_dir, kNormal, &pdf, &brdf);

      worst_length = std::max(worst_length, std::fabs(glm::length(out) - 1.0f));
      worst_lean = std::min(worst_lean, glm::dot(out, mirror_direction));
      sideways_mean.Add(static_cast<double>(glm::dot(out, sideways)));
      up_down_mean.Add(static_cast<double>(glm::dot(out, up_down)));
    }

    CHECK(worst_length < 1e-5f);               // it is a ray direction
    CHECK(worst_lean >= widest_lean - 1e-5f);  // inside the cone, on every draw
    CHECK_ESTIMATE(sideways_mean, 0.0);        // and centred in it
    CHECK_ESTIMATE(up_down_mean, 0.0);
  }

  TEST_CASE("metal: a perturbation into the surface is absorbed") {
    // At a grazing angle a wide lobe hangs partly below the horizon. Those
    // draws cannot be scattered, so the material reports no density and no
    // weight. This is the one case where a delta material returns a pdf of
    // 0, and it is deliberate: a rough metal darkens at its silhouette.
    const MetalMaterial rough(glm::vec3(1.0f), 0.8f);
    const glm::vec3 view_dir = Incident(80.0f);

    Rng rng(70u);
    int absorbed = 0;
    const int draws = 20000;

    for (int i = 0; i < draws; ++i) {
      float pdf;
      glm::vec3 brdf;
      const glm::vec3 out = rough.Sample(rng, view_dir, kNormal, &pdf, &brdf);

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

}  // TEST_SUITE bsdf/metal

//=====================================================================
// A dielectric is the one material whose scattered direction can be wrong
// while every energy measurement stays perfect: its throughput is 1 and
// the furnace is uniform, so each direction returns the same radiance.
// Nothing above would notice. These check the direction itself.

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

}  // namespace

TEST_SUITE("bsdf/dielectric") {
  TEST_CASE(
      "dielectric: the delta contract holds when transmitting and when "
      "reflecting") {
    const DielectricMaterial glass(1.5f);
    CheckDeltaContract(glass, Incident(20.0f), glm::vec3(1.0f), 80u);
    CheckDeltaContract(glass, IncidentFromBelow(80.0f), glm::vec3(1.0f), 81u);
  }

  TEST_CASE("dielectric: an index of 1 is not an interface at all") {
    // No bend anywhere, so the ray must come out exactly where it would
    // have gone unobstructed. Holds the physics at identity.
    const DielectricMaterial none(1.0f);
    for (float degrees : {0.0f, 45.0f, 89.0f}) {
      CAPTURE(degrees);
      const glm::vec3 view_dir = Incident(degrees);
      const glm::vec3 out = probe::DrawOnce(none, view_dir, 82u).direction;
      CHECK(glm::length(out + view_dir) < 1e-5f);
    }
  }

  TEST_CASE(
      "dielectric: Refract() is a unit vector wherever Snell has a solution") {
    // Asserted on the helper rather than through Sample(), which
    // normalises and would hide it. Past the critical angle refract() has
    // no answer and returns a long tangent, so the caller must not ask.
    for (const Crossing& c : kCrossings) {
      const double eta = RelativeIndex(c);
      const double sin_in =
          std::sin(glm::radians(static_cast<double>(c.degrees)));
      if (eta * sin_in > 1.0) continue;

      CAPTURE(c.ior);
      CAPTURE(c.entering);
      CAPTURE(c.degrees);

      const glm::vec3 facing = c.entering ? probe::kNormal : -probe::kNormal;
      const glm::vec3 view_dir =
          c.entering ? Incident(c.degrees) : IncidentFromBelow(c.degrees);

      CHECK(std::fabs(glm::length(
                          Refract(view_dir, facing, static_cast<float>(eta))) -
                      1.0f) < 1e-5f);
    }
  }

  TEST_CASE("dielectric: below the critical angle the ray obeys Snell") {
    // sin(theta_t) = eta * sin(theta_i), and the ray leaves through the
    // far side of the surface. A sign error in the tangential term keeps
    // the angle and flips the side, so both halves are asserted.
    for (const Crossing& c : kCrossings) {
      const double eta = RelativeIndex(c);
      const double sin_in =
          std::sin(glm::radians(static_cast<double>(c.degrees)));
      if (eta * sin_in > 1.0)
        continue;  // reflected, and checked by the case below

      CAPTURE(c.ior);
      CAPTURE(c.entering);
      CAPTURE(c.degrees);

      const DielectricMaterial mat(c.ior);
      const glm::vec3 facing = c.entering ? probe::kNormal : -probe::kNormal;
      const glm::vec3 view_dir =
          c.entering ? Incident(c.degrees) : IncidentFromBelow(c.degrees);
      const glm::vec3 out = probe::DrawOnce(mat, view_dir, 84u).direction;

      const double cos_out = glm::dot(out, facing);
      CHECK(cos_out < 0.0);  // transmitted, not reflected
      CHECK(std::fabs(std::sqrt(1.0 - cos_out * cos_out) - eta * sin_in) <
            1e-4);
    }
  }

  TEST_CASE(
      "dielectric: past the critical angle the ray reflects and only there") {
    // Total internal reflection is a property of the crossing, not of the
    // material: the same glass that can reflect a ray on the way out never
    // can on the way in. Testing the index in place of the ratio passes
    // the way out and fails the way in, which is why both appear here.
    int reflections = 0;

    for (const Crossing& c : kCrossings) {
      CAPTURE(c.ior);
      CAPTURE(c.entering);
      CAPTURE(c.degrees);

      const double eta = RelativeIndex(c);
      const double sin_in =
          std::sin(glm::radians(static_cast<double>(c.degrees)));
      const bool expected = eta * sin_in > 1.0;

      const DielectricMaterial mat(c.ior);
      const glm::vec3 facing = c.entering ? probe::kNormal : -probe::kNormal;
      const glm::vec3 view_dir =
          c.entering ? Incident(c.degrees) : IncidentFromBelow(c.degrees);
      const glm::vec3 out = probe::DrawOnce(mat, view_dir, 85u).direction;

      const bool reflected = glm::dot(out, facing) > 0.0f;
      CHECK(reflected == expected);

      if (expected) {
        ++reflections;
        CHECK(glm::length(out - Reflect(view_dir, facing)) < 1e-5f);
      }
    }

    // Both branches have to be reachable, or the equality above is vacuous.
    CHECK(reflections > 0);
    CHECK(reflections <
          static_cast<int>(sizeof(kCrossings) / sizeof(kCrossings[0])));
  }

}  // TEST_SUITE bsdf/dielectric
