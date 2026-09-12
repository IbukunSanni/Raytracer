#include "scene/Material.hpp"

#include "scene/Scattering.hpp"
#include "render/Sampling.hpp"

#include <algorithm>
#include <cmath>

Material::Material()
{}

Material::~Material()
{}

//----------------------------------------------------------------------
// LambertianMaterial

glm::vec3 LambertianMaterial::eval(const glm::vec3 &,
                                   const glm::vec3 &normal,
                                   const glm::vec3 &out) const
{
	// Below the surface is no contribution. The guard matters because next
	// event estimation calls eval() directly with a light direction that
	// pdf() would have rejected.
	if (glm::dot(normal, out) <= 0.0f)
		return glm::vec3(0.0f);
	return m_albedo / kPI;
}

float LambertianMaterial::pdf(const glm::vec3 &,
                              const glm::vec3 &normal,
                              const glm::vec3 &out) const
{
	const float c = glm::dot(normal, out);
	return c > 0.0f ? c / kPI : 0.0f;
}

glm::vec3 LambertianMaterial::sample(Rng &rng,
                                     const glm::vec3 &in,
                                     const glm::vec3 &normal,
                                     float *pdfOut,
                                     glm::vec3 *brdfOut) const
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

//----------------------------------------------------------------------
// BlinnPhongMaterial

glm::vec3 BlinnPhongMaterial::eval(const glm::vec3 &in,
                                   const glm::vec3 &normal,
                                   const glm::vec3 &out) const
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

float BlinnPhongMaterial::pdf(const glm::vec3 &in,
                              const glm::vec3 &normal,
                              const glm::vec3 &out) const
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

glm::vec3 BlinnPhongMaterial::sample(Rng &rng,
                                     const glm::vec3 &in,
                                     const glm::vec3 &normal,
                                     float *pdfOut,
                                     glm::vec3 *brdfOut) const
{
	glm::vec3 tangent, binormal;
	createOrthoNormalBasis(normal, &tangent, &binormal);

	glm::vec3 out;
	if (rng.next() < diffuseProbability()) {
		out = cosineWeightedHemiSphereSurface(rng, normal, tangent, binormal);
	} else {
		// Draw a half-vector from the Blinn-Phong power lobe, then reflect
		// `in` about it to get the scattered direction.
		const float cosThetaH =
		    std::pow(rng.next(), 1.0f / (m_shininess + 1.0f));
		const float sinThetaH =
		    std::sqrt(std::max(0.0f, 1.0f - cosThetaH * cosThetaH));
		const float phi = 2.0f * kPI * rng.next();
		const glm::vec3 h = std::cos(phi) * sinThetaH * tangent +
		                    std::sin(phi) * sinThetaH * binormal +
		                    cosThetaH * normal;
		out = reflect(in, h);
	}

	const float density = pdf(in, normal, out);
	if (pdfOut)
		*pdfOut = density;
	if (brdfOut)
		*brdfOut = density > 0.0f ? eval(in, normal, out) : glm::vec3(0.0f);
	return out;
}

float BlinnPhongMaterial::diffuseProbability() const
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

//----------------------------------------------------------------------
// MirrorMaterial

bool MirrorMaterial::isSpecular() const
{
	return true;
}

// A delta lobe carries no density: eval() and pdf() are zero everywhere,
// and sample() is the only place any of this material's behaviour lives.
glm::vec3 MirrorMaterial::eval(const glm::vec3 &,
                               const glm::vec3 &,
                               const glm::vec3 &) const
{
	return glm::vec3(0.0f);
}

float MirrorMaterial::pdf(const glm::vec3 &,
                          const glm::vec3 &,
                          const glm::vec3 &) const
{
	return 0.0f;
}

glm::vec3 MirrorMaterial::sample(Rng &,
                                 const glm::vec3 &in,
                                 const glm::vec3 &normal,
                                 float *pdfOut,
                                 glm::vec3 *brdfOut) const
{
	const glm::vec3 out = reflect(in, normal);

	// pdf = 1 with the weight folded into brdf, so
	// throughput *= brdf * cos / pdf lands on exactly m_albedo.
	if (pdfOut)
		*pdfOut = 1.0f;
	if (brdfOut)
		*brdfOut = m_albedo / std::fabs(glm::dot(normal, out));
	return out;
}


//----------------------------------------------------------------------
// MetalMaterial

bool MetalMaterial::isSpecular() const
{
	return true;
}

// A delta lobe carries no density: eval() and pdf() are zero everywhere,
// and sample() is the only place any of this material's behaviour lives.
glm::vec3 MetalMaterial::eval(const glm::vec3 &,
                              const glm::vec3 &,
                              const glm::vec3 &) const
{
	return glm::vec3(0.0f);
}

float MetalMaterial::pdf(const glm::vec3 &,
                         const glm::vec3 &,
                         const glm::vec3 &) const
{
	return 0.0f;
}

glm::vec3 MetalMaterial::sample(Rng &rng,
                                const glm::vec3 &in,
                                const glm::vec3 &normal,
                                float *pdfOut,
                                glm::vec3 *brdfOut) const
{
	// reflect() preserves length and `in` arrives normalised, so the mirror
	// direction is already a unit vector: the fuzz ball is a fixed fraction
	// of it whatever scale the caller's rays happen to use.
	const glm::vec3 out = reflect(in, normal) + m_fuzz * randomUnitVector(rng);

	// pdf = 1 with the weight folded into brdf, so
	// throughput *= brdf * cos / pdf lands on exactly m_albedo. The floor
	// matters here and not for the mirror: fuzz can tilt `out` to within
	// float noise of the tangent plane, and 1/0 * 0 would poison the pixel
	// with a NaN.
	const float cosOut = std::max(std::fabs(glm::dot(normal, out)), kEpsilon);

	if (pdfOut)
		*pdfOut = 1.0f;
	if (brdfOut)
		*brdfOut = m_albedo / cosOut;
	return out;
}