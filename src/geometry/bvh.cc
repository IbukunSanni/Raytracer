// Raytracer -- BVH build and traversal
//
// Everything mechanical lives here already: bounds of a triangle,
// statistics counters, the node array.  The two functions that make it
// a BVH -- Build() and Traverse() -- are yours to write.  Read the
// comment blocks in each before starting.

#include "geometry/bvh.h"

#include <algorithm>
#include <atomic>

#include "core/log.h"
#include "geometry/mesh.h"

namespace {
std::atomic<long long> g_nodes_visited(0);
std::atomic<long long> g_triangles_tested(0);
}  // namespace

//----------------------------------------------------------------------
AABB BVH::TriangleBounds(const std::vector<glm::vec3>& vertices,
                         const Triangle& tri) {
  AABB box;
  box.Expand(vertices[tri.v1]);
  box.Expand(vertices[tri.v2]);
  box.Expand(vertices[tri.v3]);
  return box;
}

//----------------------------------------------------------------------
void BVH::ResetStats() {
  g_nodes_visited.store(0);
  g_triangles_tested.store(0);
}

void BVH::ReportStats(const char* label) {
  LOG_DEBUG(kGeom) << "bvh " << label << ": nodes visited "
                   << g_nodes_visited.load() << ", triangles tested "
                   << g_triangles_tested.load();
}

//----------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Build a binary tree over `faces` using a MEDIAN SPLIT.
//
// State you are filling in:
//   indices_  -- a permutation of [0, faces.size()).  Start it as the
//                 identity and reorder it as you partition; leaves then
//                 refer to a contiguous slice of it.  This is why the
//                 triangles themselves never move.
//   nodes_    -- the node array.  nodes_[0] must be the root.
//   built_    -- set true only when the tree is complete and usable.
//   maxDepth_ -- deepest leaf, for the stats line.
//
// The recursion, over a slice indices_[first, first + count):
//
//   1. Compute the AABB over every triangle in the slice.  That is the
//      node's bounds -- set it whether or not you go on to split.
//
//   2. If count <= LEAF_SIZE, make a leaf: first_index = first,
//      index_count = count, children = -1.  Stop.
//
//   3. Otherwise pick a split axis.  The simple choice is the longest
//      axis of the bounds of the CENTROIDS of the triangles in the
//      slice -- note, the centroid bounds, not the triangle bounds.
//      Using triangle bounds makes a few large triangles dominate the
//      choice of axis.
//
//   4. Partition the slice about the median centroid along that axis.
//      std::nth_element is exactly the right tool: it puts the median
//      in place and everything smaller before it, in O(n), without
//      fully sorting.  Something shaped like:
//
//        std::nth_element(indices_.begin() + first,
//                         indices_.begin() + first + count / 2,
//                         indices_.begin() + first + count,
//                         [&](int a, int b) {
//                             return Centroid(a)[axis] < Centroid(b)[axis];
//                         });
//
//   5. Recurse on the two halves, record their node indices as
//      left_child / right_child, and set index_count = 0 so IsLeaf() is
//      false.
//
// One trap worth knowing about: if many triangles share a centroid
// (very common in tessellated models -- think a flat wall) the median
// split can hand every triangle to one side and recurse forever.  Guard
// it: if a split leaves either side empty, just make a leaf instead.
//
// Reserve nodes_ up front (2 * faces.size() is a safe bound for a
// binary tree with LEAF_SIZE >= 1) or take care that push_back's
// reallocation does not invalidate a reference you are holding.
//
// While built_ stays false, Mesh::IsHit uses its old linear scan, so
// the renderer keeps producing correct images throughout.
void BVH::Build(const std::vector<glm::vec3>& vertices,
                const std::vector<Triangle>& faces) {
  nodes_.clear();
  indices_.clear();
  max_depth_ = 0;
  built_ = false;

  if (faces.empty()) {
    return;
  }

  // TODO: build the tree, then set built_ = true.
  //
  // Suggested shape:
  //   indices_.resize(faces.size());
  //   std::iota(indices_.begin(), indices_.end(), 0);
  //   nodes_.reserve(2 * faces.size());
  //   BuildRecursive(0, faces.size(), 0, vertices, faces);  // add this helper
  //   to bvh.h built_ = true;

  (void)vertices;
}

//----------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Walk the tree and return the CLOSEST triangle hit in (t0, t1).
//
// The shape of it:
//
//   - Precompute inv_dir = 1 / ray.GetDirection() componentwise, once,
//     and pass it to AABB::hit.  Dividing inside the test instead means
//     three divisions per node visited.
//
//   - Keep an explicit stack of node indices (a small fixed array is
//     fine -- 64 entries covers any tree you will build here).
//     Recursion works too but is measurably slower.
//
//   - Pop a node.  If its box misses [t0, t_best], drop it.  If it is a
//     leaf, test its triangles with Mesh::IsTriangleIntersection and
//     keep the nearest.  Otherwise push both children.
//
//   - Every time you accept a closer hit, TIGHTEN t_best.  This is where
//     most of the speedup actually comes from: a shrinking t_best makes
//     later box tests fail much more often.
//
//   - Better still, push the children in FAR-then-NEAR order so the
//     near child is popped first (compare the ray's direction sign on
//     the split axis, or compare the two children's box entry
//     distances).  Finding a close hit early tightens t_best sooner.
//     Worth doing after the unordered version works.
//
// Increment g_nodesVisited and g_trianglesTested as you go -- use
// fetch_add with std::memory_order_relaxed, since several threads
// traverse at once and you only want a rough total.
//
// Returning false here means "no hit"; Mesh::IsHit only calls this when
// IsBuilt() is true, so an unfinished build is never a problem.
bool BVH::Traverse(Ray& ray, float t0, float t1,
                   const std::vector<glm::vec3>& vertices,
                   const std::vector<Triangle>& faces, BVHHit& out_hit) const {
  // TODO: stack-based descent as described above.
  (void)ray;
  (void)t0;
  (void)t1;
  (void)vertices;
  (void)faces;
  (void)out_hit;
  return false;
}
