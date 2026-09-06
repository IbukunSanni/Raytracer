#pragma once

#include "scene/Material.hpp"

#include <glm/glm.hpp>
using namespace glm;

// What a ray hit: distance along the ray, the point, the surface normal
// there, and the material to shade with. Geometry writes one; the
// integrator only reads.
class HitRecord {
public:
	HitRecord()
		: m_t(0.0f)
		, m_hitPointVec(0.0f)
		, m_normalVec(0.0f)
		, m_material(nullptr)
	{}

	float          getT()        const { return m_t; }
	const vec3 &   getHitPoint() const { return m_hitPointVec; }
	const vec3 &   getNormal()   const { return m_normalVec; }
	Material *     getMaterial() const { return m_material; }

	void setT(float tFloat)               { m_t = tFloat; }
	void setHitPoint(const vec3 & pVec)   { m_hitPointVec = pVec; }
	void setNormal(const vec3 & nVec)     { m_normalVec = nVec; }
	void setMaterial(Material * material) { m_material = material; }

	// Every primitive writes all three together.
	void setHit(float tFloat, const vec3 & pVec, const vec3 & nVec) {
		m_t = tFloat;
		m_hitPointVec = pVec;
		m_normalVec = nVec;
	}

private:
	float      m_t;
	vec3       m_hitPointVec;
	vec3       m_normalVec;
	Material * m_material;
};