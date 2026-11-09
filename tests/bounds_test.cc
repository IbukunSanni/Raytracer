// Every primitive's and node's Bounds(), checked against its own IsHit.
//
// One claim: no hit lands outside the box. A box too small renders fine
// until something culls against it, then a sliver of the object vanishes,
// so the claim is checked hit by hit on random rays rather than by eye.
//
// Rays start on a sphere well outside the object and aim at random points
// near it, so most hit and some graze. Each case also requires a fair
// number of hits, so a type that is never hit cannot pass by default.
//
// The last node case checks what culls on those boxes: the tree over a
// node's children must find the same nearest hit as the unculled scan.

#include <doctest/doctest.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <limits>
#include <memory>
#include <vector>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/aabb.h"
#include "geometry/mesh.h"
#include "geometry/primitive.h"
#include "render/sampling.h"
#include "scene/geometry_node.h"
#include "scene/scene_node.h"

namespace {

constexpr int kRays = 4000;
// Hits sit exactly on faces, so float rounding may put one a hair outside.
constexpr float kSlack = 1e-4f;

bool Contains(const AABB& box, const glm::vec3& p) {
  for (int axis = 0; axis < 3; ++axis) {
    const float slack = kSlack * std::max(1.0f, std::abs(p[axis]));
    if (p[axis] < box.min_vec[axis] - slack) return false;
    if (p[axis] > box.max_vec[axis] + slack) return false;
  }
  return true;
}

// Fires kRays random rays at `box` and checks every hit `is_hit` reports
// lies inside it. Returns the number of hits. Ray times span the shutter.
template <typename IsHit>
int CheckHitsInside(const AABB& box, IsHit is_hit, uint32_t seed) {
  REQUIRE(box.IsFinite());
  REQUIRE(!box.IsEmpty());
  Rng rng(seed);
  const glm::vec3 centre = box.Centroid();
  const glm::vec3 half = 0.5f * box.Extent();
  const float reach = 3.0f * glm::length(half) + 1.0f;
  int hits = 0;
  for (int i = 0; i < kRays; ++i) {
    const glm::vec3 origin = centre + reach * RandomUnitVector(rng);
    const glm::vec3 target =
        centre + 1.2f * half *
                     glm::vec3(rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f),
                               rng.Range(-1.0f, 1.0f));
    Ray ray(origin, target - origin, rng.Next(), &rng);
    HitRecord hit;
    if (!is_hit(ray, hit)) continue;
    ++hits;
    const glm::vec3 p = hit.GetHitPoint();
    CAPTURE(p.x);
    CAPTURE(p.y);
    CAPTURE(p.z);
    REQUIRE(Contains(box, p));
  }
  return hits;
}

template <typename P>
int CheckPrimitive(P& prim, uint32_t seed) {
  return CheckHitsInside(
      prim.Bounds(),
      [&](Ray& ray, HitRecord& hit) {
        return prim.IsHit(ray, 1e-4f, 1e6f, hit);
      },
      seed);
}

int CheckNode(SceneNode& node, uint32_t seed) {
  return CheckHitsInside(
      node.Bounds(),
      [&](Ray& ray, HitRecord& hit) {
        return node.IsHit(ray, 1e-4f, 1e6f, hit);
      },
      seed);
}

constexpr int kMinHits = kRays / 10;

}  // namespace

TEST_SUITE("bounds/primitive") {
  TEST_CASE("bounds: unit sphere and unit cube hold every hit") {
    Sphere sphere;
    CHECK(CheckPrimitive(sphere, 1) > kMinHits);
    Cube cube;
    CHECK(CheckPrimitive(cube, 2) > kMinHits);
  }

  TEST_CASE("bounds: still and moving spheres hold every hit") {
    NonhierSphere still(glm::vec3(1.0f, -2.0f, 0.5f), 1.5);
    CHECK(CheckPrimitive(still, 3) > kMinHits);
    NonhierSphere moving(glm::vec3(-1.0f, 0.0f, 0.0f),
                         glm::vec3(2.0f, 1.0f, -0.5f), 0.75);
    CHECK(CheckPrimitive(moving, 4) > kMinHits);
  }

  TEST_CASE("bounds: box holds every hit") {
    NonhierBox box(glm::vec3(-0.5f, 2.0f, 1.0f), 2.5);
    CHECK(CheckPrimitive(box, 5) > kMinHits);
    Box slab_box(glm::vec3(1.0f, -2.0f, 0.5f), glm::vec3(-0.5f, 1.0f, 3.0f));
    CHECK(CheckPrimitive(slab_box, 14) > kMinHits);
  }

  TEST_CASE("bounds: tilted and axis-aligned quads hold every hit") {
    Quad tilted(glm::vec3(1.0f, -0.5f, 0.25f), glm::vec3(2.0f, 0.0f, 0.5f),
                glm::vec3(0.5f, 1.5f, -0.25f));
    CHECK(CheckPrimitive(tilted, 6) > kMinHits);
    // Flat in z: the padding is what gives this box any volume.
    Quad flat(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(2.0f, 0.0f, 0.0f),
              glm::vec3(0.0f, 1.0f, 0.0f));
    const AABB box = flat.Bounds();
    CHECK(box.max_vec.z > box.min_vec.z);
    CHECK(CheckPrimitive(flat, 7) > kMinHits);
  }

  TEST_CASE("bounds: mesh is its triangles' box") {
    std::vector<glm::vec3> verts = {
        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.5f, 0.5f, -1.0f)};
    const std::vector<glm::vec3> faces = {
        glm::vec3(0, 1, 2), glm::vec3(0, 1, 3), glm::vec3(0, 2, 3),
        glm::vec3(1, 2, 3)};
    Mesh mesh(verts, faces);
    const AABB box = mesh.Bounds();
    CHECK(box.min_vec == glm::vec3(0.0f, 0.0f, -1.0f));
    CHECK(box.max_vec == glm::vec3(2.0f, 1.5f, 0.0f));
    CHECK(CheckPrimitive(mesh, 8) > kMinHits);
  }

  TEST_CASE("bounds: medium holds every stop inside its boundary") {
    // Dense, so most rays that enter stop somewhere inside.
    ConstantMedium smoke(
        std::make_unique<NonhierSphere>(glm::vec3(0.5f, 0.0f, -1.0f), 1.25),
        4.0);
    CHECK(CheckPrimitive(smoke, 9) > kMinHits);
  }

  TEST_CASE("bounds: a primitive without its own box is infinite") {
    Primitive bare;
    CHECK(!bare.Bounds().IsFinite());
    CHECK(!bare.Bounds().Transformed(glm::scale(glm::vec3(0.0f))).IsFinite());
  }
}

TEST_SUITE("bounds/node") {
  TEST_CASE("bounds: rotated, scaled, translated geometry holds every hit") {
    GeometryNode node("cube", new Cube());
    node.Scale(glm::vec3(2.0f, 0.5f, 1.0f));
    node.Rotate('y', 30.0f);
    node.Rotate('x', -20.0f);
    node.Translate(glm::vec3(3.0f, -1.0f, 2.0f));
    CHECK(CheckNode(node, 10) > kMinHits);
  }

  TEST_CASE("bounds: a tree of nodes holds every hit") {
    SceneNode root("root");
    root.Rotate('z', 15.0f);
    root.Translate(glm::vec3(-1.0f, 0.0f, 0.5f));

    auto* sphere = new GeometryNode("sphere", new Sphere());
    sphere->Scale(glm::vec3(0.5f, 1.5f, 0.5f));
    sphere->Translate(glm::vec3(2.0f, 0.0f, 0.0f));
    root.AddChild(sphere);

    auto* arm = new SceneNode("arm");
    arm->Rotate('y', 45.0f);
    auto* quad = new GeometryNode(
        "quad", new Quad(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                         glm::vec3(0.0f, 0.0f, 1.0f)));
    quad->Translate(glm::vec3(0.0f, 1.0f, 0.0f));
    arm->AddChild(quad);
    root.AddChild(arm);

    CHECK(CheckNode(root, 11) > kMinHits);
  }

  TEST_CASE("bounds: the child tree finds the scan's nearest hit") {
    // Two copies of one field of spheres: `tree` is cached, so it gets a
    // tree; `scan` never is, so it tests every child, unculled.
    SceneNode tree("tree");
    SceneNode scan("scan");
    Rng place(12);
    for (int i = 0; i < 40; ++i) {
      const glm::vec3 at(place.Range(-4.0f, 4.0f), place.Range(-1.0f, 1.0f),
                         place.Range(-4.0f, 4.0f));
      const float radius = place.Range(0.2f, 0.8f);
      for (SceneNode* root : {&tree, &scan}) {
        auto* sphere = new GeometryNode("sphere", new Sphere());
        sphere->Scale(glm::vec3(radius));
        sphere->Translate(at);
        root->AddChild(sphere);
      }
    }
    tree.CacheBounds();
    REQUIRE(tree.HasChildTree());
    REQUIRE(!scan.HasChildTree());

    Rng rng(13);
    int hits = 0;
    for (int i = 0; i < kRays; ++i) {
      const glm::vec3 origin = 12.0f * RandomUnitVector(rng);
      const glm::vec3 target(rng.Range(-4.0f, 4.0f), rng.Range(-1.0f, 1.0f),
                             rng.Range(-4.0f, 4.0f));
      Ray ray(origin, target - origin, 0.0f, &rng);
      HitRecord tree_hit, scan_hit;
      const bool tree_is_hit = tree.IsHit(ray, 1e-4f, 1e6f, tree_hit);
      const bool scan_is_hit = scan.IsHit(ray, 1e-4f, 1e6f, scan_hit);
      REQUIRE(tree_is_hit == scan_is_hit);
      if (!tree_is_hit) continue;
      ++hits;
      // Exact: a sphere's t does not depend on the t1 it was tested with.
      REQUIRE(tree_hit.GetT() == scan_hit.GetT());
    }
    CHECK(hits > kMinHits);
  }

  TEST_CASE("bounds: a node with no children is empty") {
    SceneNode lonely("lonely");
    lonely.Translate(glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(lonely.Bounds().IsEmpty());
  }
}
