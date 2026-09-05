// Raytracer -- BVH build and traversal
//
// Everything mechanical lives here already: bounds of a triangle,
// statistics counters, the node array.  The two functions that make it
// a BVH -- build() and traverse() -- are yours to write.  Read the
// comment blocks in each before starting.

#include "geometry/BVH.hpp"
#include "geometry/Mesh.hpp"

#include <atomic>
#include <algorithm>
#include <iostream>

namespace {
	std::atomic<long long> g_nodesVisited(0);
	std::atomic<long long> g_trianglesTested(0);
}

//----------------------------------------------------------------------
AABB BVH::triangleBounds(const std::vector<glm::vec3> & vertices,
                         const Triangle & tri)
{
	AABB box;
	box.expand(vertices[tri.v1]);
	box.expand(vertices[tri.v2]);
	box.expand(vertices[tri.v3]);
	return box;
}

//----------------------------------------------------------------------
void BVH::resetStats()
{
	g_nodesVisited.store(0);
	g_trianglesTested.store(0);
}

void BVH::reportStats(const char * label)
{
	std::cout << "[BVH] " << label
	          << " nodes visited = " << g_nodesVisited.load()
	          << ", triangles tested = " << g_trianglesTested.load()
	          << std::endl;
}

//----------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Build a binary tree over `faces` using a MEDIAN SPLIT.
//
// State you are filling in:
//   m_indices  -- a permutation of [0, faces.size()).  Start it as the
//                 identity and reorder it as you partition; leaves then
//                 refer to a contiguous slice of it.  This is why the
//                 triangles themselves never move.
//   m_nodes    -- the node array.  m_nodes[0] must be the root.
//   m_built    -- set true only when the tree is complete and usable.
//   m_maxDepth -- deepest leaf, for the stats line.
//
// The recursion, over a slice m_indices[first, first + count):
//
//   1. Compute the AABB over every triangle in the slice.  That is the
//      node's bounds -- set it whether or not you go on to split.
//
//   2. If count <= LEAF_SIZE, make a leaf: firstIndex = first,
//      indexCount = count, children = -1.  Stop.
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
//        std::nth_element(m_indices.begin() + first,
//                         m_indices.begin() + first + count / 2,
//                         m_indices.begin() + first + count,
//                         [&](int a, int b) {
//                             return centroid(a)[axis] < centroid(b)[axis];
//                         });
//
//   5. Recurse on the two halves, record their node indices as
//      leftChild / rightChild, and set indexCount = 0 so isLeaf() is
//      false.
//
// One trap worth knowing about: if many triangles share a centroid
// (very common in tessellated models -- think a flat wall) the median
// split can hand every triangle to one side and recurse forever.  Guard
// it: if a split leaves either side empty, just make a leaf instead.
//
// Reserve m_nodes up front (2 * faces.size() is a safe bound for a
// binary tree with LEAF_SIZE >= 1) or take care that push_back's
// reallocation does not invalidate a reference you are holding.
//
// While m_built stays false, Mesh::isHit uses its old linear scan, so
// the renderer keeps producing correct images throughout.
void BVH::build(const std::vector<glm::vec3> & vertices,
                const std::vector<Triangle> & faces)
{
	m_nodes.clear();
	m_indices.clear();
	m_maxDepth = 0;
	m_built = false;

	if (faces.empty()) {
		return;
	}

	// TODO: build the tree, then set m_built = true.
	//
	// Suggested shape:
	//   m_indices.resize(faces.size());
	//   std::iota(m_indices.begin(), m_indices.end(), 0);
	//   m_nodes.reserve(2 * faces.size());
	//   buildRecursive(0, faces.size(), 0, vertices, faces);  // add this helper to BVH.hpp
	//   m_built = true;

	(void) vertices;
}

//----------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Walk the tree and return the CLOSEST triangle hit in (t0, t1).
//
// The shape of it:
//
//   - Precompute invDir = 1 / ray.getDirection() componentwise, once,
//     and pass it to AABB::hit.  Dividing inside the test instead means
//     three divisions per node visited.
//
//   - Keep an explicit stack of node indices (a small fixed array is
//     fine -- 64 entries covers any tree you will build here).
//     Recursion works too but is measurably slower.
//
//   - Pop a node.  If its box misses [t0, tBest], drop it.  If it is a
//     leaf, test its triangles with Mesh::isTriangleIntersection and
//     keep the nearest.  Otherwise push both children.
//
//   - Every time you accept a closer hit, TIGHTEN tBest.  This is where
//     most of the speedup actually comes from: a shrinking tBest makes
//     later box tests fail much more often.
//
//   - Better still, push the children in FAR-then-NEAR order so the
//     near child is popped first (compare the ray's direction sign on
//     the split axis, or compare the two children's box entry
//     distances).  Finding a close hit early tightens tBest sooner.
//     Worth doing after the unordered version works.
//
// Increment g_nodesVisited and g_trianglesTested as you go -- use
// fetch_add with std::memory_order_relaxed, since several threads
// traverse at once and you only want a rough total.
//
// Returning false here means "no hit"; Mesh::isHit only calls this when
// isBuilt() is true, so an unfinished build is never a problem.
bool BVH::traverse(Ray & ray,
                   float t0,
                   float t1,
                   const std::vector<glm::vec3> & vertices,
                   const std::vector<Triangle> & faces,
                   BVHHit & outHit) const
{
	// TODO: stack-based descent as described above.
	(void) ray; (void) t0; (void) t1;
	(void) vertices; (void) faces; (void) outHit;
	return false;
}
