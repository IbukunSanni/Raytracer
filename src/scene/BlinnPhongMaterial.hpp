#pragma once

#include <algorithm>
#include <cmath>

#include "scene/Material.hpp"
#include "render/Sampling.hpp"

// Modified Blinn-Phong BRDF: a Lambertian diffuse lobe plus a normalised
// half-vector power lobe. With kd + ks <= 1 it conserves energy.
//
// `in` (toward the previous vertex) and `out` (the scattered direction)
// are unit vectors that both point away from the surface and live in the
// hemisphere around `normal`.
class BlinnPhongMaterial : public Material
{
public:
    BlinnPhongMaterial(const glm::vec3 &kd, const glm::vec3 &ks,
                       double shininess)
        : m_kd(kd), m_ks(ks), m_shininess(static_cast<float>(shininess))
    {
    }

    glm::vec3 eval(const glm::vec3 &in, const glm::vec3 &normal,
                   const glm::vec3 &out) const override
    {
        if (glm::dot(normal, in) <= 0.0f || glm::dot(normal, out) <= 0.0f)
            return glm::vec3(0.0f);

        glm::vec3 spec(0.0f);
        const glm::vec3 h = in + out;
        const float hLen2 = glm::dot(h, h);
        if (hLen2 > kHalfVecEps) {
            const float nDotH = glm::dot(normal, h * glm::inversesqrt(hLen2));
            if (nDotH > 0.0f)
                spec = m_ks * ((m_shininess + 2.0f) / (8.0f * kPI)) *
                       std::pow(nDotH, m_shininess);
        }
        return m_kd / kPI + spec;
    }

    float pdf(const glm::vec3 &in, const glm::vec3 &normal,
              const glm::vec3 &out) const override
    {
        const float nDotOut = glm::dot(normal, out);
        if (nDotOut <= 0.0f)
            return 0.0f;

        const float pDiffuse = diffuseProbability();
        const float pdfD = nDotOut / kPI;

        float pdfS = 0.0f;
        const glm::vec3 h = in + out;
        const float hLen2 = glm::dot(h, h);
        if (hLen2 > kHalfVecEps) {
            const glm::vec3 hUnit = h * glm::inversesqrt(hLen2);
            const float nDotH = glm::dot(normal, hUnit);
            const float inDotH = glm::dot(in, hUnit);
            if (nDotH > 0.0f && inDotH > 0.0f)
                pdfS = (m_shininess + 1.0f) / (2.0f * kPI) *
                       std::pow(nDotH, m_shininess) / (4.0f * inDotH);
        }

        return pDiffuse * pdfD + (1.0f - pDiffuse) * pdfS;
    }

    glm::vec3 sample(Rng &rng, const glm::vec3 &in, const glm::vec3 &normal,
                     float *pdfOut, glm::vec3 *brdfOut) const override
    {
        glm::vec3 tangent, binormal;
        createOrthoNormalBasis(normal, &tangent, &binormal);

        glm::vec3 out;
        if (rng.next() < diffuseProbability()) {
            out = cosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);
        } else {
            // Draw a half-vector from the Blinn-Phong power lobe, then
            // reflect `in` about it to get the scattered direction.
            const float cosThetaH =
                std::pow(rng.next(), 1.0f / (m_shininess + 1.0f));
            const float sinThetaH =
                std::sqrt(std::max(0.0f, 1.0f - cosThetaH * cosThetaH));
            const float phi = 2.0f * kPI * rng.next();
            const glm::vec3 h = std::cos(phi) * sinThetaH * tangent +
                                std::sin(phi) * sinThetaH * binormal +
                                cosThetaH * normal;
            out = 2.0f * glm::dot(in, h) * h - in;
        }

        const float density = pdf(in, normal, out);
        if (pdfOut)
            *pdfOut = density;
        if (brdfOut)
            *brdfOut = density > 0.0f ? eval(in, normal, out) : glm::vec3(0.0f);
        return out;
    }

private:
    // Luminance-weighted split between sampling the two lobes. Clamped so
    // neither active lobe is starved, but a fully black lobe is skipped.
    float diffuseProbability() const
    {
        const glm::vec3 luma(0.2126f, 0.7152f, 0.0722f);
        const float d = glm::dot(m_kd, luma);
        const float s = glm::dot(m_ks, luma);
        if (s <= 0.0f)
            return 1.0f;
        if (d <= 0.0f)
            return 0.0f;
        return std::min(0.9f, std::max(0.1f, d / (d + s)));
    }

    static constexpr float kHalfVecEps = 1e-8f;

    glm::vec3 m_kd;
    glm::vec3 m_ks;
    float m_shininess;
};
