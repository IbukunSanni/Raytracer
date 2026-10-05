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

// Per-thread tallies, published once when the thread exits. An atomic add
// per ray costs more than the traversal itself with 20 threads on one line.
// Render threads are joined before ReportStats, so every tally is in by then.
struct ThreadStats {
  long long nodes_visited = 0;
  long long triangles_tested = 0;
  ~ThreadStats() {
    g_nodes_visited.fetch_add(nodes_visited, std::memory_order_relaxed);
    g_triangles_tested.fetch_add(triangles_tested, std::memory_order_relaxed);
  }
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

void BVH::CountTrianglesTested(long long n) {
  t_stats.triangles_tested += n;
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
// >>> YOU IMPLEMENT THIS <<<
//
// Walk the tree and return the CLOSEST triangle hit in (t0_float, t1_float).
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
//   - Pop a node.  If its box misses [t0_float, t_best], drop it.  If it is a
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
bool BVH::Traverse(Ray& ray, float t0_float, float t1_float,
                   const std::vector<glm::vec3>& vertices,
                   const std::vector<Triangle>& faces, BVHHit& out_hit) const {
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
      for (int k = leaf.first_index; k < leaf.first_index + leaf.index_count;
           ++k) {
        int face_index = indices_[k];
        const Triangle& face = faces[face_index];
        float pot_t = 0.0f;
        // Passing t_best as the far limit means only a closer hit passes.
        if (Mesh::IsTriangleIntersection(ray, vertices[face.v1],
                                         vertices[face.v2], vertices[face.v3],
                                         pot_t, t0_float, t_best)) {
          hit = true;
          t_best = pot_t;
          out_hit.face_index = face_index;
          out_hit.t = pot_t;
        }
      }
      triangles_tested += leaf.index_count;
    } else {
      stack[stack_size++] = nodes_[i].right_child;
      stack[stack_size++] = nodes_[i].left_child;
    }
  }

  t_stats.nodes_visited += nodes_visited;
  t_stats.triangles_tested += triangles_tested;
  return hit;
}
