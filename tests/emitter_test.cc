// The area lights a scene graph hands over, checked against the graph itself.
//
// Three claims. A lamp's node hands its radiance to the hits on its front, and
// nothing to those on its back. The walk finds the quads that emit and nothing
// else. And what it returns is in world space: a ray hits the returned quad
// exactly when the same ray through the graph hits the lamp, and the density
// the quad reports uses the distance and cosine the graph measures, whatever
// transforms stand between the root and the lamp.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/primitive.h"
#include "render/sampling.h"
#include "scene/emitters.h"
#include "scene/geometry_node.h"
#include "scene/material.h"
#include "scene/scene_node.h"
#include "scene/texture.h"

namespace {

const glm::vec3 kQ(1.0f, -0.5f, 0.25f);
const glm::vec3 kU(2.0f, 0.0f, 0.5f);
const glm::vec3 kV(0.5f, 1.5f, -0.25f);

constexpr int kRays = 20000;
constexpr float kMaxT = std::numeric_limits<float>::max();

// The transforms on the way down to the lamp: root, then one node, then the
// lamp's own. They do not commute, so composing them out of order, or skipping
// one, moves the quad away from where the graph draws it.
struct Chain {
  std::string label;  // a std::string, so a failure prints it, not its address
  glm::mat4 root;
  glm::mat4 arm;
  glm::mat4 lamp;
};

std::vector<Chain> Chains() {
  const glm::mat4 id(1.0f);
  const glm::mat4 turn =
      glm::rotate(glm::radians(50.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::mat4 tilt =
      glm::rotate(glm::radians(-30.0f), glm::vec3(1.0f, 0.0f, 0.0f));
  return {
      {"identity", id, id, id},
      {"moved", glm::translate(glm::vec3(4.0f, -2.0f, 1.0f)), id, id},
      {"uniform scale", id, glm::scale(glm::vec3(2.5f)), id},
      {"stretched and turned, at every level",
       glm::translate(glm::vec3(-3.0f, 1.0f, 2.0f)) * tilt,
       turn * glm::scale(glm::vec3(1.0f, 2.0f, 1.0f)),
       glm::scale(glm::vec3(3.0f, 0.5f, 1.0f))},
      {"mirrored", id, glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f)), turn},
  };
}

// A lamp as a scene builds one: a surface that reflects nothing, with the
// emission on its node.
GeometryNode* Lamp(const std::string& name, Primitive* primitive,
                   Material* black) {
  GeometryNode* node = new GeometryNode(name, primitive, black);
  node->SetEmission(std::make_shared<SolidColor>(glm::vec3(1.0f)));
  return node;
}

}  // namespace

TEST_SUITE("emitters") {
  TEST_CASE("emitters: a lamp glows from its front and not from its back") {
    LambertianMaterial black(glm::vec3(0.0f));
    LambertianMaterial grey(glm::vec3(0.8f));
    const glm::vec3 radiance(4.0f, 2.0f, 1.0f);  // above 1, as lamps are

    // Its front is u x v, which is +z.
    const glm::vec3 q(-1.0f, -1.0f, 0.0f);
    const glm::vec3 u(2.0f, 0.0f, 0.0f);
    const glm::vec3 v(0.0f, 2.0f, 0.0f);
    GeometryNode lamp("lamp", new Quad(q, u, v), &black);
    lamp.SetEmission(std::make_shared<SolidColor>(radiance));
    GeometryNode wall("wall", new Quad(q, u, v), &grey);

    Ray from_front(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                   0.0f, nullptr);
    Ray from_back(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                  0.0f, nullptr);

    HitRecord front;
    HitRecord back;
    REQUIRE(lamp.IsHit(from_front, 1e-4f, kMaxT, front));
    REQUIRE(lamp.IsHit(from_back, 1e-4f, kMaxT, back));
    CHECK(front.Emitted() == radiance);
    CHECK(back.Emitted() == glm::vec3(0.0f));

    // A node that is not a lamp gives off nothing, whatever its material.
    HitRecord lit;
    REQUIRE(wall.IsHit(from_front, 1e-4f, kMaxT, lit));
    CHECK(lit.GetEmission() == nullptr);
    CHECK(lit.Emitted() == glm::vec3(0.0f));
  }

  TEST_CASE("emitters: the walk finds the quads that emit, and nothing else") {
    LambertianMaterial black(glm::vec3(0.0f));
    LambertianMaterial wall(glm::vec3(0.5f));

    SceneNode root("root");
    root.AddChild(Lamp("lamp", new Quad(kQ, kU, kV), &black));
    // Black but not a lamp: emission is on the node, not in the colour.
    root.AddChild(new GeometryNode("soot", new Quad(kQ, kU, kV), &black));
    root.AddChild(new GeometryNode("wall", new Quad(kQ, kU, kV), &wall));
    root.AddChild(Lamp("bulb", new NonhierSphere(kQ, 1.0), &black));
    root.AddChild(new GeometryNode("bare", new Quad(kQ, kU, kV)));

    SceneNode* group = new SceneNode("group");
    group->AddChild(Lamp("deep", new Quad(kQ, kU, kV), &black));
    root.AddChild(group);

    CHECK(CollectEmitters(root).size() == 2);
  }

  TEST_CASE("emitters: a scene without an emissive quad gives an empty list") {
    LambertianMaterial wall(glm::vec3(0.5f));
    SceneNode root("root");
    root.AddChild(new GeometryNode("wall", new Quad(kQ, kU, kV), &wall));
    CHECK(CollectEmitters(root).empty());
  }

  TEST_CASE(
      "emitters: the quad is where the graph draws it, at its world area") {
    uint32_t seed = 60u;
    for (const Chain& chain : Chains()) {
      CAPTURE(chain.label);

      LambertianMaterial black(glm::vec3(0.0f));
      SceneNode root("root");
      root.SetTransform(chain.root);
      SceneNode* arm = new SceneNode("arm");
      arm->SetTransform(chain.arm);
      GeometryNode* lamp_node = Lamp("lamp", new Quad(kQ, kU, kV), &black);
      lamp_node->SetTransform(chain.lamp);
      arm->AddChild(lamp_node);
      root.AddChild(arm);

      const std::vector<Quad> emitters = CollectEmitters(root);
      REQUIRE(emitters.size() == 1);
      const Quad& quad = emitters.front();

      const glm::mat4 to_world = chain.root * chain.arm * chain.lamp;
      const glm::vec3 u_world(to_world * glm::vec4(kU, 0.0f));
      const glm::vec3 v_world(to_world * glm::vec4(kV, 0.0f));
      const glm::vec3 cross_world = glm::cross(u_world, v_world);
      CHECK(quad.Area() ==
            doctest::Approx(glm::length(cross_world)).epsilon(1e-5));

      // From a point off the quad, aim at a ball a little wider than the quad
      // so that plenty of rays hit it and plenty pass beside its edges.
      const glm::vec3 centre(to_world *
                             glm::vec4(kQ + 0.5f * kU + 0.5f * kV, 1.0f));
      const float reach = glm::length(u_world) + glm::length(v_world);
      const glm::vec3 origin =
          centre + reach * (glm::normalize(cross_world) +
                            0.4f * glm::normalize(u_world));

      Rng rng(seed++);
      int hits = 0;
      int disagree = 0;
      double worst_density_error = 0.0;
      for (int i = 0; i < kRays; ++i) {
        const glm::vec3 target = centre + reach * SampleUnitSphere(rng);
        const glm::vec3 w = glm::normalize(target - origin);

        Ray ray(origin, w, 0.0f, nullptr);
        HitRecord hit;
        const bool graph_hit = root.IsHit(ray, 1e-4f, kMaxT, hit);
        const double pdf = quad.Pdf(origin, w);

        // A ray this close to an edge can round to either side of it.
        if (graph_hit != (pdf > 0.0)) ++disagree;
        if (!graph_hit || pdf <= 0.0) continue;
        ++hits;

        // The ray is unit length, so t is the distance.
        const double distance = hit.GetT();
        const double cosine =
            std::abs(glm::dot(glm::normalize(hit.GetNormal()), w));
        const double expected =
            distance * distance / (cosine * static_cast<double>(quad.Area()));
        worst_density_error =
            std::max(worst_density_error, std::abs(pdf / expected - 1.0));
      }

      CAPTURE(hits);
      CAPTURE(disagree);
      CAPTURE(worst_density_error);
      CHECK(hits > kRays / 20);
      CHECK(disagree <= kRays / 1000);
      CHECK(worst_density_error < 1e-2);
    }
  }
}  // TEST_SUITE emitters
