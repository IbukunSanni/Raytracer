// The BVH, checked against the linear scan it replaced -- no scene, no
// renderer, no image.
//
// One claim: for every ray, the tree finds the same nearest hit as testing
// every triangle. A tree that drops a triangle still renders, just with a
// hole or a surface seen through, so the claim is checked ray by ray rather
// than by eye. It is checked for both splits and both traversals, on the
// meshes the scenes use and on the 12-triangle box, whose axis-aligned
// faces give leaves a box of zero thickness.
//
// Rays come from three places, because each stresses a different part of
// the box test: from outside the mesh aimed into it, from inside it in any
// direction (as bounce rays start), and axis-aligned from a vertex's own
// coordinate, where a zero direction component meets an origin on a slab
// plane.
//
// BVH_TEST_RAYS=N changes the ray count per mesh, split and traversal.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <limits>
#include <string>
#include <vector>

#include "core/ray.h"
#include "geometry/aabb.h"
#include "geometry/bvh.h"
#include "geometry/mesh.h"
#include "render/sampling.h"

namespace {

int RayCount() {
  const char* value = std::getenv("BVH_TEST_RAYS");
  return (value != nullptr && std::atoi(value) > 0) ? std::atoi(value) : 5000;
}

glm::vec3 RandomInBox(Rng& rng, const AABB& box) {
  return box.min_vec + glm::vec3(rng.Next(), rng.Next(), rng.Next()) *
                           (box.max_vec - box.min_vec);
}

glm::vec3 RandomDirection(Rng& rng) {
  return glm::vec3(rng.Next(), rng.Next(), rng.Next()) * 2.0f - 1.0f;
}

// The i-th test ray, cycling through the three kinds in the header.
Ray TestRay(int i, Rng& rng, const AABB& bounds,
            const std::vector<glm::vec3>& vertices) {
  const glm::vec3 centre = bounds.Centroid();
  const float radius = glm::length(bounds.max_vec - bounds.min_vec);
  Ray ray;
  switch (i % 3) {
    case 0: {
      const glm::vec3 origin =
          centre + glm::normalize(RandomDirection(rng)) * radius;
      ray.SetOrigin(origin);
      ray.SetDirection(RandomInBox(rng, bounds) - origin);
      break;
    }
    case 1:
      ray.SetOrigin(RandomInBox(rng, bounds));
      ray.SetDirection(RandomDirection(rng));
      break;
    default: {
      const int axis = static_cast<int>(rng.Next() * 3.0f) % 3;
      const glm::vec3& vertex =
          vertices[static_cast<size_t>(rng.Next() * vertices.size()) %
                   vertices.size()];
      glm::vec3 origin = centre + glm::normalize(RandomDirection(rng)) * radius;
      glm::vec3 direction = RandomInBox(rng, bounds) - origin;
      origin[axis] = vertex[axis];
      direction[axis] = 0.0f;
      ray.SetOrigin(origin);
      ray.SetDirection(direction);
      break;
    }
  }
  return ray;
}

// The nearest hit over every face, as Mesh::LinearScan finds it, but also
// reporting which face.
bool LinearNearest(Ray& ray, float t_far, const std::vector<glm::vec3>& v,
                   const std::vector<Triangle>& faces, float* t, int* face) {
  bool hit = false;
  float t_best = t_far;
  for (size_t i = 0; i < faces.size(); ++i) {
    float t_hit = 0.0f;
    if (Mesh::IsTriangleIntersection(ray, v[faces[i].v1], v[faces[i].v2],
                                     v[faces[i].v3], t_hit, kEpsilon, t_best)) {
      hit = true;
      t_best = t_hit;
      *face = static_cast<int>(i);
    }
  }
  *t = t_best;
  return hit;
}

// Fires RayCount() rays at `mesh` through a tree built with `split` and
// walked by `traversal`, and compares each with a linear scan under the
// tolerance BVH_VERIFY uses.
//
// The triangle test is not exact at an edge: a ray that passes a few float
// steps outside a triangle can still be accepted. The tree's box test is
// exact there and rejects it, so the two disagree without the tree being
// wrong. Such a graze is recognised by the hit triangle's own box rejecting
// the ray; it is counted and reported, and only other disagreements fail.
void CheckAgainstLinearScan(Mesh& mesh, BVHSplit split,
                            BVHTraversal traversal) {
  const std::vector<glm::vec3>& vertices = mesh.Vertices();
  const std::vector<Triangle>& faces = mesh.Faces();
  REQUIRE(!faces.empty());

  BVH tree;
  tree.Build(vertices, faces, split);
  REQUIRE(tree.IsBuilt());

  AABB bounds;
  for (const glm::vec3& v : vertices) bounds.Expand(v);

  Rng rng(12345u);
  const float t_far = std::numeric_limits<float>::max();
  int mismatches = 0;
  int grazes = 0;
  for (int i = 0; i < RayCount(); ++i) {
    Ray ray = TestRay(i, rng, bounds, vertices);

    float linear_t = 0.0f;
    int linear_face = -1;
    const bool linear_hit =
        LinearNearest(ray, t_far, vertices, faces, &linear_t, &linear_face);

    BVHHit tree_hit;
    const bool hit = (traversal == BVHTraversal::kRecursive)
                         ? tree.TraverseRecursive(ray, kEpsilon, t_far,
                                                  vertices, faces, tree_hit)
                         : tree.TraverseIterative(ray, kEpsilon, t_far,
                                                  vertices, faces, tree_hit);

    if (hit == linear_hit &&
        (!hit || std::fabs(tree_hit.t - linear_t) <= 1e-4f)) {
      continue;
    }
    if (linear_hit && (!hit || linear_t < tree_hit.t)) {
      const AABB box = BVH::TriangleBounds(
          vertices, faces[static_cast<size_t>(linear_face)]);
      if (!box.Hit(ray.GetOrigin(), 1.0f / ray.GetDirection(), kEpsilon,
                   linear_t)) {
        ++grazes;
        continue;
      }
    }
    if (mismatches++ < 3) {
      char ray_text[256];
      std::snprintf(ray_text, sizeof(ray_text),
                    "ray %d from %a %a %a along %a %a %a", i, ray.GetOrigin().x,
                    ray.GetOrigin().y, ray.GetOrigin().z, ray.GetDirection().x,
                    ray.GetDirection().y, ray.GetDirection().z);
      MESSAGE(ray_text, ": linear hit=", linear_hit,
              " t=", linear_hit ? linear_t : -1.0f, ", tree hit=", hit,
              " t=", hit ? tree_hit.t : -1.0f);
    }
  }
  if (grazes > 0) MESSAGE(grazes, " edge grazes accepted by the linear scan");
  CHECK(mismatches == 0);
}

void CheckEveryTree(Mesh& mesh) {
  for (BVHSplit split : {BVHSplit::kMedian, BVHSplit::kSAH}) {
    for (BVHTraversal traversal :
         {BVHTraversal::kRecursive, BVHTraversal::kIterative}) {
      const std::string split_name = BVH::SplitName(split);
      const std::string traversal_name = BVH::TraversalName(traversal);
      CAPTURE(split_name);
      CAPTURE(traversal_name);
      CheckAgainstLinearScan(mesh, split, traversal);
    }
  }
}

}  // namespace

//=====================================================================
TEST_SUITE("bvh/linear-scan") {
  TEST_CASE("bvh: the box's flat leaves match the linear scan") {
    std::vector<glm::vec3> corners = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1},
                                      {0, 0, 1}, {0, 1, 0}, {1, 1, 0},
                                      {1, 1, 1}, {0, 1, 1}};
    const std::vector<glm::vec3> faces = {
        {0, 1, 2}, {0, 2, 3}, {0, 7, 4}, {0, 3, 7}, {0, 4, 5}, {0, 5, 1},
        {6, 2, 1}, {6, 1, 5}, {6, 5, 4}, {6, 4, 7}, {6, 7, 3}, {6, 3, 2}};
    Mesh box(corners, faces);
    CheckEveryTree(box);
  }

  TEST_CASE("bvh: cow.obj matches the linear scan") {
    Mesh mesh("assets/models/cow.obj");
    CheckEveryTree(mesh);
  }

  TEST_CASE("bvh: Drone.obj matches the linear scan") {
    Mesh mesh("assets/models/Drone.obj");
    CheckEveryTree(mesh);
  }

  TEST_CASE("bvh: spaceship.obj matches the linear scan") {
    Mesh mesh("assets/models/spaceship.obj");
    CheckEveryTree(mesh);
  }

  TEST_CASE("bvh: Lamborghini_Aventador.obj matches the linear scan") {
    Mesh mesh("assets/models/Lamborghini_Aventador.obj");
    CheckEveryTree(mesh);
  }
}
