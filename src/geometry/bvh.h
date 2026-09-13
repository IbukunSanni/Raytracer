// Raytracer -- bounding volume hierarchy over a triangle mesh
//
// The problem it solves: Mesh::IsHit currently tests every ray against
// every triangle, so cost is O(rays * triangles).  cow.obj is 5804
// triangles; macho-cows.lua has six of them.  Every shadow ray, every
// reflection ray, every pixel, pays for all of it.
//
// A BVH turns that into roughly O(rays * log triangles).  Triangles are
// partitioned into a binary tree; each node stores an AABB enclosing
// its subtree.  A ray that misses a node's box cannot hit anything
// below it, so the entire subtree is skipped with one cheap test.
//
// The tree is built ONCE per mesh at load time and then read
// concurrently by all render threads, so traversal must not mutate it.

#ifndef RAYTRACER_SRC_GEOMETRY_BVH_H_
#define RAYTRACER_SRC_GEOMETRY_BVH_H_

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "core/ray.h"
#include "geometry/aabb.h"

struct Triangle;  // from mesh.h

// A node is either interior (two children, no triangles) or a leaf
// (a contiguous run of triangle indices, no children).
struct BVHNode {
  AABB bounds;
  int left_child = -1;  // index into nodes_; -1 on a leaf
  int right_child = -1;
  int first_index = 0;  // offset into indices_; leaves only
  int index_count = 0;  // 0 on an interior node

  bool IsLeaf() const { return index_count > 0; }
};

// Result of a successful traversal.
struct BVHHit {
  int face_index = -1;
  float t = 0.0f;
};

class BVH {
 public:
  // Build over the given triangle list.  Safe to call on an empty
  // mesh.  After this returns, IsBuilt() tells you whether traversal
  // is usable; Mesh falls back to a linear scan when it is not, so a
  // half-finished BVH never breaks the renderer.
  void Build(const std::vector<glm::vec3>& vertices,
             const std::vector<Triangle>& faces);

  bool IsBuilt() const { return built_; }

  size_t NodeCount() const { return nodes_.size(); }
  int MaxDepth() const { return max_depth_; }

  // Find the closest triangle hit in (t0, t1).  Returns false if
  // nothing was hit.  Must be const and must not touch shared mutable
  // state -- every render thread calls this at once.
  bool Traverse(Ray& ray, float t0, float t1,
                const std::vector<glm::vec3>& vertices,
                const std::vector<Triangle>& faces, BVHHit& out_hit) const;

  // Provided for you: the AABB around one triangle.
  static AABB TriangleBounds(const std::vector<glm::vec3>& vertices,
                             const Triangle& tri);

  // Provided for you: how many leaf/interior nodes were visited and
  // how many triangles were tested on the last render.  Useful for
  // proving the tree is doing something.  Thread-safe counters.
  static void ResetStats();
  static void ReportStats(const char* label);

  // How many triangles a leaf is allowed to hold before we stop
  // splitting.  Smaller => deeper tree, more traversal, fewer
  // triangle tests.  4 is a reasonable starting point; try changing
  // it once the thing works and measure.
  static const int kLeafSize = 4;

 private:
  std::vector<BVHNode> nodes_;
  std::vector<int> indices_;  // permutation of face indices
  bool built_ = false;
  int max_depth_ = 0;
};

#endif  // RAYTRACER_SRC_GEOMETRY_BVH_H_
