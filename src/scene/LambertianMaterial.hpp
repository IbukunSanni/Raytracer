#pragma once

#include "scene/Material.hpp"
#include "render/Sampling.hpp"

class LambertianMaterial : public Material
{
public:
    explicit LambertianMaterial(const glm::vec3 &albedo) : m_albedo(albedo) {}

    glm::vec3 eval(const glm::vec3 &, const glm::vec3 &,
                   const glm::vec3 &) const override
    {
        return m_albedo / kPI;
    }

    float pdf(const glm::vec3 &, const glm::vec3 &normal,
              const glm::vec3 &out) const override
    {
        const float c = glm::dot(normal, out);
        return c > 0.0f ? c / kPI : 0.0f;
    }

    glm::vec3 sample(Rng &rng, const glm::vec3 &in, const glm::vec3 &normal,
                     float *pdfOut, glm::vec3 *brdfOut) const override
    {
        glm::vec3 tangent, binormal;
        createOrthoNormalBasis(normal, &tangent, &binormal);
        const glm::vec3 dir =
            cosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);

        if (pdfOut)
            *pdfOut = glm::dot(normal, dir) / kPI;
        if (brdfOut)
            *brdfOut = eval(in, normal, dir);
        return dir;
    }

private:
    glm::vec3 m_albedo;
};