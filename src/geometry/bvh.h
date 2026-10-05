// Raytracer -- bounding volume hierarchy over a triangle mesh

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

// Which path Mesh::IsHit takes. One binary runs all three, so a
// comparison between them never also compares two builds.
enum class BVHTraversal { kLinear, kRecursive, kIterative };

// Result of a successful traversal.
struct BVHHit {
  int face_index = -1;
  float t = 0.0f;
};



class BVH {
 public:
  // Build over the given triangle list.  Safe to call on an empty
  // mesh.
  void Build(const std::vector<glm::vec3>& vertices,
             const std::vector<Triangle>& faces);

  bool IsBuilt() const { return built_; }

  size_t NodeCount() const { return nodes_.size(); }
  int MaxDepth() const { return max_depth_; }

  // Find the closest triangle hit in (t0_float, t1_float).  Returns false if
  // nothing was hit.  Must be const and must not touch shared mutable
  // state, every render thread calls this at once.
  bool Traverse(Ray& ray, float t0_float, float t1_float,
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

  // Counts triangles tested by code that is not the tree, so the
  // linear scan's cost is on the same scale as the tree's and the two
  // are comparable. Call once per scan with the whole face count.
  static void CountTrianglesTested(long long n);

  // BVH_TRAVERSAL=linear|recursive|iterative, read once. Unset means
  // linear; an unknown value exits, since a benchmark that silently
  // measured the wrong path is worse than one that did not run.
  static BVHTraversal Traversal();
  static const char* TraversalName(BVHTraversal mode);

  // How many triangles a leaf is allowed to hold before we stop
  // splitting.  Smaller => deeper tree, more traversal, fewer
  // triangle tests.  4 is a reasonable starting point; try changing
  // it once the thing works and measure.
  static const int kLeafSize = 4;

  // Entries in the fixed traversal stack. Build() refuses a tree deeper
  // than this allows; a median split over a million faces is ~18 deep.
  static const int kStackSize = 64;

  int BuildRecursive(int first, int count, int depth,
                        const std::vector<AABB>& bbox_triangles);

 private:
  std::vector<BVHNode> nodes_;
  std::vector<int> indices_;  // permutation of face indices
  bool built_ = false;
  int max_depth_ = 0;
};

#endif  // RAYTRACER_SRC_GEOMETRY_BVH_H_
