#pragma once

#include <glm/glm.hpp>

// Scattering primitives shared between materials.
//
// The physics lives here rather than inside any one material, so that
// adding a material is lobe-selection logic and nothing else. Fresnel and
// refraction join this file with the dielectric.

// Reflect `in` about `axis`. Both point AWAY from the surface, and so does
// the result. The axis is the surface normal for a mirror and the
// half-vector for a glossy lobe -- one formula, two uses.
inline glm::vec3 reflect(const glm::vec3 & in, const glm::vec3 & axis)
{
	return 2.0f * glm::dot(in, axis) * axis - in;
}
