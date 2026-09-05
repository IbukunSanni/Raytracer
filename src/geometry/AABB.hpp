// Raytracer -- axis-aligned bounding box
//
// The building block of the BVH.  An AABB is the cheapest useful
// "does the ray go anywhere near this?" test: if a ray misses the box,
// it misses everything inside the box, so a whole subtree can be
// skipped without touching a single triangle.
//
// Everything mechanical (growing boxes, centroids, surface area) is
// provided.  The intersection test itself is yours.

#pragma once

#include "core/Ray.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <limits>

struct AABB {
	glm::vec3 minVec;
	glm::vec3 maxVec;

	// An empty box: deliberately inverted, so the first expand() call
	// snaps it onto the real data.
	AABB()
		: minVec( std::numeric_limits<float>::max() )
		, maxVec( -std::numeric_limits<float>::max() )
	{}

	AABB(const glm::vec3 & lo, const glm::vec3 & hi)
		: minVec(lo), maxVec(hi)
	{}

	bool isEmpty() const { return minVec.x > maxVec.x; }

	// Grow to contain a point.
	void expand(const glm::vec3 & p) {
		minVec = glm::min(minVec, p);
		maxVec = glm::max(maxVec, p);
	}

	// Grow to contain another box.
	void expand(const AABB & other) {
		if (other.isEmpty()) return;
		minVec = glm::min(minVec, other.minVec);
		maxVec = glm::max(maxVec, other.maxVec);
	}

	glm::vec3 extent() const {
		return isEmpty() ? glm::vec3(0.0f) : (maxVec - minVec);
	}

	glm::vec3 centroid() const { return 0.5f * (minVec + maxVec); }

	// Index of the longest axis: 0 = x, 1 = y, 2 = z.  The usual choice
	// for a median split, because splitting the long axis keeps child
	// boxes closer to cubes and so keeps their surface area down.
	int longestAxis() const {
		const glm::vec3 e = extent();
		if (e.x >= e.y && e.x >= e.z) return 0;
		return (e.y >= e.z) ? 1 : 2;
	}

	// Needed later if you upgrade the split heuristic to SAH.  Not used
	// by a median split.
	float surfaceArea() const {
		const glm::vec3 e = extent();
		return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
	}

	// -----------------------------------------------------------------
	// >>> YOU IMPLEMENT THIS <<<
	//
	// The slab test.  Return true if the ray overlaps this box anywhere
	// in the parameter interval [t0, t1].
	//
	// The idea: an AABB is the intersection of three "slabs", one per
	// axis -- the x slab is everything between x = minVec.x and
	// x = maxVec.x, and so on.  For each axis, work out the interval of
	// t over which the ray is inside that slab:
	//
	//     tNear = (minVec[a] - origin[a]) / direction[a]
	//     tFar  = (maxVec[a] - origin[a]) / direction[a]
	//
	// If direction[a] is negative those two come out swapped, so order
	// them.  Then intersect all three intervals with each other and
	// with [t0, t1]; the ray hits the box exactly when what is left is
	// non-empty, i.e. the running max of the tNears never exceeds the
	// running min of the tFars.
	//
	// Two details worth getting right:
	//
	//   - A ray exactly parallel to an axis gives direction[a] == 0 and
	//     a division by zero.  IEEE floats make this work out on their
	//     own: you get +inf or -inf, and the comparisons still behave,
	//     PROVIDED the origin is not exactly on the slab boundary (that
	//     case yields 0/0 = NaN, and every NaN comparison is false).
	//     Multiplying by a precomputed 1/direction rather than dividing
	//     is both faster and the conventional way to write it.
	//
	//   - Do NOT normalize the ray direction here.  The rest of the renderer
	//     carries unnormalized directions where t is expressed in units
	//     of the direction vector's length, and the BVH has to agree
	//     with the triangle test about what t means.
	//
	// Returning true unconditionally, as it does now, is CONSERVATIVE:
	// it never culls anything, so the BVH still produces correct images
	// -- just with no speedup at all.  That is deliberate.  Build the
	// tree first, confirm the picture is unchanged, then implement this
	// and watch the render time fall.
	bool hit(const glm::vec3 & origin,
	         const glm::vec3 & invDir,
	         float t0,
	         float t1) const
	{
		// TODO: real slab test.
		(void) origin; (void) invDir; (void) t0; (void) t1;
		return true;
	}
};
