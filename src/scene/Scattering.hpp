#pragma once

#include <glm/glm.hpp>
#include <cmath>

// Scattering primitives shared between materials.
//
// The physics lives here rather than inside any one material, so that
// adding a material is lobe-selection logic and nothing else. Fresnel and
// refraction join this file with the dielectric.

// Reflect `viewDir` about `normal`. Both point AWAY from the surface, and so
// does the result. The normal is the surface normal for a mirror and the
// half-vector for a glossy lobe -- one formula, two uses.
inline glm::vec3 reflect(const glm::vec3 &viewDir, const glm::vec3 &normal)
{
	return 2.0f * glm::dot(viewDir, normal) * normal - viewDir;
}

inline glm::vec3 refract(const glm::vec3& viewDir,
                         const glm::vec3& normal,
                         float indexRatio)
{
    float cos_theta = std::fmin(glm::dot(viewDir, normal), 1.0);

    glm::vec3 perpendicular =
        indexRatio * (viewDir - cos_theta * normal);

    glm::vec3 parallel =
        (float)-std::sqrt(std::fmax(0.0, 1.0 - glm::dot(perpendicular, perpendicular)))
        * normal;

    return perpendicular + parallel;
}
