// A4 -- bounding volume hierarchy over a triangle mesh
//
// The problem it solves: Mesh::isHit currently tests every ray against
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

#pragma once

#include "AABB.hpp"
#include "RayTracer.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct Triangle; // from Mesh.hpp

// A node is either interior (two children, no triangles) or a leaf
// (a contiguous run of triangle indices, no children).
struct BVHNode {
	AABB   bounds;
	int    leftChild  = -1; // index into m_nodes; -1 on a leaf
	int    rightChild = -1;
	int    firstIndex = 0;  // offset into m_indices; leaves only
	int    indexCount = 0;  // 0 on an interior node

	bool isLeaf() const { return indexCount > 0; }
};

// Result of a successful traversal.
struct BVHHit {
	int   faceIndex = -1;
	float t         = 0.0f;
};

class BVH {
public:
	// Build over the given triangle list.  Safe to call on an empty
	// mesh.  After this returns, isBuilt() tells you whether traversal
	// is usable; Mesh falls back to a linear scan when it is not, so a
	// half-finished BVH never breaks the renderer.
	void build(const std::vector<glm::vec3> & vertices,
	           const std::vector<Triangle> & faces);

	bool isBuilt() const { return m_built; }

	size_t nodeCount() const { return m_nodes.size(); }
	int    maxDepth()  const { return m_maxDepth; }

	// Find the closest triangle hit in (t0, t1).  Returns false if
	// nothing was hit.  Must be const and must not touch shared mutable
	// state -- every render thread calls this at once.
	bool traverse(RayTracer & ray,
	              float t0,
	              float t1,
	              const std::vector<glm::vec3> & vertices,
	              const std::vector<Triangle> & faces,
	              BVHHit & outHit) const;

	// Provided for you: the AABB around one triangle.
	static AABB triangleBounds(const std::vector<glm::vec3> & vertices,
	                           const Triangle & tri);

	// Provided for you: how many leaf/interior nodes were visited and
	// how many triangles were tested on the last render.  Useful for
	// proving the tree is doing something.  Thread-safe counters.
	static void resetStats();
	static void reportStats(const char * label);

	// How many triangles a leaf is allowed to hold before we stop
	// splitting.  Smaller => deeper tree, more traversal, fewer
	// triangle tests.  4 is a reasonable starting point; try changing
	// it once the thing works and measure.
	static const int LEAF_SIZE = 4;

private:
	std::vector<BVHNode> m_nodes;
	std::vector<int>     m_indices;   // permutation of face indices
	bool                 m_built = false;
	int                  m_maxDepth = 0;
};
