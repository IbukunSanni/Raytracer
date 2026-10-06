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
  int split_axis = 0;   // interior only; left holds the lower centroids

  bool IsLeaf() const { return index_count > 0; }
};

// Which path Mesh::IsHit takes. One binary runs all three, so a
// comparison between them never also compares two builds.
enum class BVHTraversal { kLinear, kRecursive, kIterative };

// Where Build() cuts a node: at the median along the longest axis, or where
// the surface area heuristic says. Selected at run time for the same reason.
enum class BVHSplit { kMedian, kSAH };

// Result of a successful traversal.
struct BVHHit {
  int face_index = -1;
  float t = 0.0f;
};

class BVH {
 public:
  // Build over the given triangle list, cutting nodes with `split`. Safe
  // to call on an empty mesh.
  void Build(const std::vector<glm::vec3>& vertices,
             const std::vector<Triangle>& faces, BVHSplit split);

  bool IsBuilt() const { return built_; }

  size_t NodeCount() const { return nodes_.size(); }
  int MaxDepth() const { return max_depth_; }

  // Set by Build(). Build time falls outside the render timer, so this is
  // the only place it is measured; it stays 0 unless RT_STATS=1.
  double BuildMs() const { return build_ms_; }
  int LeafCount() const { return leaf_count_; }
  int MaxLeafSize() const { return max_leaf_size_; }

  // Find the closest triangle hit in [t0_float, t1_float]; false if none.
  // Both visit the same nodes in the same order, so their counts must match.
  // Every render thread calls these at once, so neither touches shared state.
  bool TraverseRecursive(Ray& ray, float t0_float, float t1_float,
                         const std::vector<glm::vec3>& vertices,
                         const std::vector<Triangle>& faces,
                         BVHHit& out_hit) const;
  bool TraverseIterative(Ray& ray, float t0_float, float t1_float,
                         const std::vector<glm::vec3>& vertices,
                         const std::vector<Triangle>& faces,
                         BVHHit& out_hit) const;

  // Provided for you: the AABB around one triangle.
  static AABB TriangleBounds(const std::vector<glm::vec3>& vertices,
                             const Triangle& tri);

  // Nodes visited and triangles tested on the last render. Counted only
  // when RT_STATS=1, since counting slows the render it measures; with it
  // off, ReportStats says so instead of printing zeros.
  static void ResetStats();
  static void ReportStats(const char* label);

  // The frame totals ReportStats prints, for the bench record, plus the
  // build time of every mesh in the scene. All zero unless RT_STATS=1.
  struct FrameStats {
    long long calls = 0;
    long long nodes_visited = 0;
    long long triangles_tested = 0;
    double build_ms = 0.0;
  };
  static FrameStats Totals();

  // Counts triangles tested by code that is not the tree, so the
  // linear scan's cost is on the same scale as the tree's and the two
  // are comparable. Call once per scan with the whole face count.
  static void CountTrianglesTested(long long n);

  // Adds this thread's tallies to the frame totals and zeroes them. Every
  // render thread calls it once before returning, or its work goes unreported.
  static void FlushThreadStats();

  // BVH_TRAVERSAL=linear|recursive|iterative, read once. Unset means
  // iterative; an unknown value exits, since a benchmark that silently
  // measured the wrong path is worse than one that did not run.
  static BVHTraversal Traversal();
  static const char* TraversalName(BVHTraversal mode);

  // BVH_SPLIT=median|sah, read once at the first build. Unset means median;
  // an unknown value exits, as for BVH_TRAVERSAL.
  static BVHSplit Split();
  static const char* SplitName(BVHSplit split);

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
  // Binned SAH: partitions indices_[first, first + count) at the cheapest
  // cut and reports its axis and left size. False when no axis has any
  // centroid extent, so the caller falls back to the median.
  bool SAHSplit(int first, int count, const AABB& centroid_bounds,
                const std::vector<AABB>& bbox_triangles, int* split_axis,
                int* left_count);

  std::vector<BVHNode> nodes_;
  std::vector<int> indices_;  // permutation of face indices
  bool built_ = false;
  int max_depth_ = 0;
  BVHSplit split_ = BVHSplit::kMedian;  // the split Build() was given
  double build_ms_ = 0.0;
  int leaf_count_ = 0;
  int max_leaf_size_ = 0;
};

#endif  // RAYTRACER_SRC_GEOMETRY_BVH_H_
