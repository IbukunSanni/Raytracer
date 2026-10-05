// Raytracer -- BVH build and traversal
//

#include "geometry/bvh.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "geometry/mesh.h"

namespace {
std::atomic<long long> g_nodes_visited(0);
std::atomic<long long> g_triangles_tested(0);

// Per-thread tallies, published by FlushThreadStats. An atomic add per ray
// costs more than the traversal itself with 20 threads on one line. No
// destructor: MinGW's emulated TLS runs them unreliably at thread exit.
struct ThreadStats {
  long long nodes_visited = 0;
  long long triangles_tested = 0;
};
thread_local ThreadStats t_stats;
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
  // Parse the mode here, before the render threads start, so a bad value
  // exits from the main thread rather than from inside a band.
  (void)Traversal();
  g_nodes_visited.store(0);
  g_triangles_tested.store(0);
}

void BVH::CountTrianglesTested(long long n) { t_stats.triangles_tested += n; }

void BVH::FlushThreadStats() {
  g_nodes_visited.fetch_add(t_stats.nodes_visited, std::memory_order_relaxed);
  g_triangles_tested.fetch_add(t_stats.triangles_tested,
                               std::memory_order_relaxed);
  t_stats = ThreadStats();
}

void BVH::ReportStats(const char* label) {
  LOG_DEBUG(kGeom) << "bvh " << label << ": traversal "
                   << TraversalName(Traversal()) << ", nodes visited "
                   << g_nodes_visited.load() << ", triangles tested "
                   << g_triangles_tested.load();
}

//----------------------------------------------------------------------
static BVHTraversal ParseTraversal() {
  const char* value = std::getenv("BVH_TRAVERSAL");
  if (value == nullptr || *value == '\0') return BVHTraversal::kLinear;
  for (BVHTraversal mode : {BVHTraversal::kLinear, BVHTraversal::kRecursive,
                            BVHTraversal::kIterative}) {
    if (std::strcmp(value, BVH::TraversalName(mode)) == 0) return mode;
  }
  LOG_ERROR(kGeom) << "unknown BVH_TRAVERSAL '" << value
                   << "'; expected linear, recursive or iterative";
  std::exit(EXIT_FAILURE);
}

BVHTraversal BVH::Traversal() {
  static const BVHTraversal kMode = ParseTraversal();
  return kMode;
}

const char* BVH::TraversalName(BVHTraversal mode) {
  switch (mode) {
    case BVHTraversal::kLinear:
      return "linear";
    case BVHTraversal::kRecursive:
      return "recursive";
    case BVHTraversal::kIterative:
      return "iterative";
  }
  return "?";
}

int BVH::BuildRecursive(int first, int count, int depth,
                        const std::vector<AABB>& bbox_triangles) {
  int node_index = static_cast<int>(nodes_.size());
  nodes_.emplace_back();

  AABB bounds;
  AABB centroid_bounds;

  for (int i = first; i < first + count; ++i) {
    const AABB& box = bbox_triangles[indices_[i]];
    // i is a position; indices_[i] is a face
    bounds.Expand(box);
    centroid_bounds.Expand(box.Centroid());
  }

  nodes_[node_index].bounds = bounds;
  max_depth_ = std::max(max_depth_, depth);

  // Few enough faces: I'm a leaf. My faces are indices_[first, first + count).
  if (count <= kLeafSize) {
    nodes_[node_index].first_index = first;
    nodes_[node_index].index_count = count;
    return node_index;
  }

  // Sort my slice along my longest axis and cut it in half.
  int longest_axis = centroid_bounds.LongestAxis();
  int left_count = count / 2;
  int right_count = count - left_count;
  int right_first = first + left_count;

  auto by_centroid = [&](int face_a, int face_b) {
    return bbox_triangles[face_a].Centroid()[longest_axis] <
           bbox_triangles[face_b].Centroid()[longest_axis];
  };
  // std::nth_element(begin + first, begin + right_first, begin + first +
  // count, by_centroid) gives the same halves in O(n), each left unsorted.
  std::sort(indices_.begin() + first, indices_.begin() + first + count,
            by_centroid);

  nodes_[node_index].split_axis = longest_axis;
  nodes_[node_index].left_child =
      BuildRecursive(first, left_count, depth + 1, bbox_triangles);
  nodes_[node_index].right_child =
      BuildRecursive(right_first, right_count, depth + 1, bbox_triangles);
  return node_index;
}

void BVH::Build(const std::vector<glm::vec3>& vertices,
                const std::vector<Triangle>& faces) {
  nodes_.clear();
  indices_.clear();
  max_depth_ = 0;
  built_ = false;

  if (faces.empty()) {
    return;
  }

  // Each face's box is computed once here; the recursion reads it by
  // face index, so reordering indices_ never invalidates it.
  std::vector<AABB> bbox_triangles(faces.size());
  indices_.resize(faces.size());
  // Fewer than 2n nodes for n faces, so no emplace_back reallocates.
  nodes_.reserve(2 * faces.size());

  for (size_t i = 0; i < indices_.size(); ++i) {
    indices_[i] = i;
    bbox_triangles[indices_[i]] = TriangleBounds(vertices, faces[i]);
  }

  BuildRecursive(0, static_cast<int>(faces.size()), 0, bbox_triangles);

  // Traversal pops one node and pushes two per level, so it holds at most
  // max_depth_ + 1 entries. Past that it would write off the stack array.
  if (max_depth_ + 1 > kStackSize) {
    LOG_ERROR(kGeom) << "bvh depth " << max_depth_
                     << " overflows the traversal stack of " << kStackSize;
    std::exit(EXIT_FAILURE);
  }
  built_ = true;
}

//----------------------------------------------------------------------
namespace {
// Tests every triangle in a leaf and keeps the nearest. Passing t_best as
// the far limit means only a hit closer than the current best passes.
bool TestLeaf(const BVHNode& leaf, const std::vector<int>& indices,
              const std::vector<glm::vec3>& vertices,
              const std::vector<Triangle>& faces, Ray& ray, float t0_float,
              float& t_best, BVHHit& out_hit) {
  bool hit = false;
  for (int k = leaf.first_index; k < leaf.first_index + leaf.index_count; ++k) {
    int face_index = indices[k];
    const Triangle& face = faces[face_index];
    float pot_t = 0.0f;
    if (Mesh::IsTriangleIntersection(ray, vertices[face.v1], vertices[face.v2],
                                     vertices[face.v3], pot_t, t0_float,
                                     t_best)) {
      hit = true;
      t_best = pot_t;
      out_hit.face_index = face_index;
      out_hit.t = pot_t;
    }
  }
  return hit;
}

// A ray heading down the split axis meets the right (higher) child first.
bool RightIsNear(const BVHNode& node, const glm::vec3& inv_dir) {
  return inv_dir[node.split_axis] < 0.0f;
}

// Everything one recursive walk reads or updates, so each level passes a
// single reference rather than a dozen arguments.
struct RecursiveWalk {
  const std::vector<BVHNode>& nodes;
  const std::vector<int>& indices;
  const std::vector<glm::vec3>& vertices;
  const std::vector<Triangle>& faces;
  Ray& ray;
  BVHHit& out_hit;
  glm::vec3 inv_dir;
  float t0_float;
  float t_best;
  bool hit;
  long long nodes_visited;
  long long triangles_tested;
};

void VisitNode(RecursiveWalk& walk, int node_index) {
  ++walk.nodes_visited;
  const BVHNode& node = walk.nodes[node_index];
  if (!node.bounds.Hit(walk.ray.GetOrigin(), walk.inv_dir, walk.t0_float,
                       walk.t_best)) {
    return;
  }

  if (node.IsLeaf()) {
    if (TestLeaf(node, walk.indices, walk.vertices, walk.faces, walk.ray,
                 walk.t0_float, walk.t_best, walk.out_hit)) {
      walk.hit = true;
    }
    walk.triangles_tested += node.index_count;
    return;
  }

  if (RightIsNear(node, walk.inv_dir)) {
    VisitNode(walk, node.right_child);
    VisitNode(walk, node.left_child);
  } else {
    VisitNode(walk, node.left_child);
    VisitNode(walk, node.right_child);
  }
}
}  // namespace

/**
 * Walks the tree depth-first by recursion and returns the closest triangle
 * hit in [t0_float, t1_float].
 *
 * Visits the same nodes in the same order as TraverseIterative: near child
 * first, each box tested against t_best when it is reached. Their node and
 * triangle counts must therefore match to the digit.
 *
 * @return true and fills out_hit if any triangle was hit; false otherwise,
 *         including when no tree was built.
 */
bool BVH::TraverseRecursive(Ray& ray, float t0_float, float t1_float,
                            const std::vector<glm::vec3>& vertices,
                            const std::vector<Triangle>& faces,
                            BVHHit& out_hit) const {
  if (!built_) {
    return false;
  }

  RecursiveWalk walk{nodes_,
                     indices_,
                     vertices,
                     faces,
                     ray,
                     out_hit,
                     1.0f / ray.GetDirection(),
                     t0_float,
                     t1_float,
                     false,
                     0,
                     0};
  VisitNode(walk, 0);

  t_stats.nodes_visited += walk.nodes_visited;
  t_stats.triangles_tested += walk.triangles_tested;
  return walk.hit;
}

/**
 * Walks the tree depth-first with an explicit stack and returns the closest
 * triangle hit in [t0_float, t1_float].
 *
 * t_best starts at t1_float and shrinks with every accepted hit. It bounds
 * both the box test and the triangle test, so once a near hit is found most
 * remaining boxes are culled. Children are visited near first, judged by the
 * ray's direction along the node's split axis, so t_best shrinks sooner.
 *
 * The stack is a fixed array of kStackSize entries; Build() rejects a tree
 * too deep for it. Work is tallied in locals and added to this thread's
 * stats once per call.
 *
 * @return true and fills out_hit if any triangle was hit; false otherwise,
 *         including when no tree was built.
 */
bool BVH::TraverseIterative(Ray& ray, float t0_float, float t1_float,
                            const std::vector<glm::vec3>& vertices,
                            const std::vector<Triangle>& faces,
                            BVHHit& out_hit) const {
  if (!built_) {
    return false;
  }

  bool hit = false;
  float t_best = t1_float;
  // Counted locally and added to t_stats once per call, not per node.
  long long nodes_visited = 0;
  long long triangles_tested = 0;
  glm::vec3 inv_dir = 1.0f / ray.GetDirection();

  // Build() guarantees max_depth_ + 1 <= kStackSize, the most this holds.
  int stack[kStackSize];
  int stack_size = 0;
  stack[stack_size++] = 0;

  while (stack_size > 0) {
    int i = stack[--stack_size];
    ++nodes_visited;

    bool is_hit =
        nodes_[i].bounds.Hit(ray.GetOrigin(), inv_dir, t0_float, t_best);
    if (!is_hit) {
      continue;
    }

    if (nodes_[i].IsLeaf()) {
      const BVHNode& leaf = nodes_[i];
      if (TestLeaf(leaf, indices_, vertices, faces, ray, t0_float, t_best,
                   out_hit)) {
        hit = true;
      }
      triangles_tested += leaf.index_count;
    } else {
      // Push the far child first so the near one pops first.
      const BVHNode& node = nodes_[i];
      if (RightIsNear(node, inv_dir)) {
        stack[stack_size++] = node.left_child;
        stack[stack_size++] = node.right_child;
      } else {
        stack[stack_size++] = node.right_child;
        stack[stack_size++] = node.left_child;
      }
    }
  }

  t_stats.nodes_visited += nodes_visited;
  t_stats.triangles_tested += triangles_tested;
  return hit;
}
