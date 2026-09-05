// Termm--Fall 2020

#include "core/Log.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>

#include <glm/ext.hpp>


// (OBJ parsing is done inline in Mesh(const std::string&) below)
#include "geometry/Mesh.hpp"

using namespace glm;
using namespace std;
// TODO: confirm const is good
static const float EPS = 0.00001f;

Mesh::Mesh( const std::string& fname )
	: m_vertices()
	, m_faces()
{
	std::string code;
	double vx, vy, vz;
	size_t s1, s2, s3;

	std::ifstream ifs( fname.c_str() );
	while( ifs >> code ) {
		if( code == "v" ) {
			ifs >> vx >> vy >> vz;
			m_vertices.push_back( glm::vec3( vx, vy, vz ) );
		} else if( code == "f" ) {
			ifs >> s1 >> s2 >> s3;
			m_faces.push_back( Triangle( s1 - 1, s2 - 1, s3 - 1 ) );
		}
	}

	m_bvh.build(m_vertices, m_faces);
	LOG_DEBUG(GEOM) << "mesh " << fname << ": " << m_faces.size() << " faces, bvh "
	                << (m_bvh.isBuilt() ? "built" : "not built (linear scan)");
}

std::ostream& operator<<(std::ostream& out, const Mesh& mesh)
{
  out << "mesh {";
  /*
  
  for( size_t idx = 0; idx < mesh.m_verts.size(); ++idx ) {
  	const MeshVertex& v = mesh.m_verts[idx];
  	out << glm::to_string( v.m_position );
	if( mesh.m_have_norm ) {
  	  out << " / " << glm::to_string( v.m_normal );
	}
	if( mesh.m_have_uv ) {
  	  out << " / " << glm::to_string( v.m_uv );
	}
  }

*/
  out << "}";
  return out;
}

bool Mesh::isTriangleIntersection(Ray &ray,vec3 vert0, vec3 vert1, vec3 vert2,float &potT1Float,float t0Float,float t1Float){
	// cout << "Mesh::isTriangleIntersection() called" << endl;
	// Define all vectors, (eVec,dVec) for ray and (aVec,bVec,cVec) for triangle
	vec3 eVec = ray.getOrigin();
	vec3 dVec = ray.getDirection();

	vec3 aVec = vert0;
	vec3 bVec = vert1;
	vec3 cVec = vert2; 

	// Ax = b, where x is unknown
	// Declare elements for A
	float a = (aVec.x - bVec.x);
	float b = (aVec.y - bVec.y);
	float c = (aVec.z - bVec.z);
	float d = (aVec.x - cVec.x);
	float e = (aVec.y - cVec.y);
	float f = (aVec.z - cVec.z);
	float g = (dVec.x);
	float h = (dVec.y);
	float i = (dVec.z);

	// Declare elements for b
	float j = (aVec.x - eVec.x);
	float k = (aVec.y - eVec.y);
	float l = (aVec.z - eVec.z);
	
	// Denominator for cramer's rule
	float M = a*(e*i - h*f) + b *(g *f -d *i) + c *(d*h - e*g);

	//compute t
	potT1Float = (-1 ) * (1/M) * (f*(a*k - j*b) + e*(j *c - a *l) + d*(b*l - k*c));
	if (potT1Float< t0Float || potT1Float>t1Float){
		// cout << "Mesh::isTriangleIntersection() left t false" << endl;
		return false;
	}

	// compute gamma
	float gamma = (1/M) * (i*(a*k - j *b) + h*(j *c - a *l) + g*(b*l - k*c));
	if(gamma< EPS || gamma > 1){
		// cout << "Mesh::isTriangleIntersection() left gamma false" << endl;
		return false;
	}

	// compute beta
	float beta = (1/M) * (j*(e*i - h*f) + k*(g*f - d*i) + l*(d*h -e*g));
	if(beta < EPS || (beta > 1-gamma)){
		// cout << "Mesh::isTriangleIntersection() left beta false" << endl;
		return false;
	}

	// cout << "Mesh::isTriangleIntersection() left true" << endl;
	return true;
}


bool Mesh::linearScan(Ray & ray,float t0Float,float t1Float, HitRecord &record ) const {
	bool hit = false;
	vec3 normalVec = vec3();
	float newT1float = t1Float;
	// Traverse every face looking for the closest hit.
	for (auto face: m_faces){
		float potT1Float = 0.0f;
		if (isTriangleIntersection(ray, m_vertices[face.v1], m_vertices[face.v2], m_vertices[face.v3], potT1Float,t0Float,newT1float)){
			hit = true;
			newT1float = potT1Float;
			vec3 faceVec1 = m_vertices[face.v1] - m_vertices[face.v2];
			vec3 faceVec2 = m_vertices[face.v2] - m_vertices[face.v3];
			normalVec = cross(faceVec1,faceVec2);
		}
	}
	if (!hit){
		return false;
	}
	// Flipping the normals
	if (dot(ray.getDirection(),normalVec)> 0){
		normalVec = -normalVec;
	}

	record.t = newT1float;
	record.normalVec = normalVec;
	record.hitPointVec = ray.getPointAtT(record.t);
	record.material = nullptr;
	return hit;
}

// Set BVH_VERIFY=1 in the environment to run BOTH paths on every ray
// and report any disagreement. Slow, but it is the fastest way to find a
// BVH bug: a tree that is merely inefficient still renders correctly,
// while one that drops triangles produces holes you may not notice.
static bool bvhVerifyEnabled() {
	static const bool on = (std::getenv("BVH_VERIFY") != nullptr);
	return on;
}

bool Mesh::isHit(Ray & ray,float t0Float,float t1Float, HitRecord &record ){
	if (RENDER_BOUNDING_VOLUMES >= 1){
		// Debug view: draw the mesh as its bounding sphere instead of its
		// geometry. This is a visualisation, not an acceleration test.
		AABB box;
		for (auto vert: m_vertices) box.expand(vert);
		vec3 c = box.centroid();
		float r = glm::length(box.maxVec - c);

		vec3 eMinusCVec = ray.getOrigin() - c;
		vec3 dVec = ray.getDirection();
		double A = (double) dot(dVec,dVec);
		double B = (double) (2 * dot(dVec,eMinusCVec));
		double C = (double) (dot(eMinusCVec,eMinusCVec) - (r *r));
		double roots[2];
		size_t numRoots = quadraticRoots(A,B,C,roots);
		float tFloat = 0;
		switch (numRoots){
			case 0: return false;
			case 1: tFloat = (float)roots[0]; break;
			default: tFloat = (float) glm::min(roots[0],roots[1]); break;
		}
		if (tFloat <= t0Float || t1Float <= tFloat) return false;
		record.t = tFloat;
		record.hitPointVec = ray.getPointAtT(tFloat);
		record.normalVec = record.hitPointVec - c;
		return true;
	}

	// No usable tree yet -> exhaustive scan. Correct, just slow.
	if (!m_bvh.isBuilt()){
		return linearScan(ray, t0Float, t1Float, record);
	}

	BVHHit bvhHit;
	bool hit = m_bvh.traverse(ray, t0Float, t1Float, m_vertices, m_faces, bvhHit);

	if (bvhVerifyEnabled()){
		HitRecord refRecord;
		bool refHit = linearScan(ray, t0Float, t1Float, refRecord);
		if (refHit != hit || (refHit && std::abs(refRecord.t - bvhHit.t) > 1e-4f)){
			LOG_ERROR(GEOM) << "bvh mismatch: linear hit=" << refHit
			                << " t=" << (refHit ? refRecord.t : -1.0f)
			                << " | bvh hit=" << hit
			                << " t=" << (hit ? bvhHit.t : -1.0f);
		}
	}

	if (!hit) return false;

	const Triangle & face = m_faces[bvhHit.faceIndex];
	vec3 faceVec1 = m_vertices[face.v1] - m_vertices[face.v2];
	vec3 faceVec2 = m_vertices[face.v2] - m_vertices[face.v3];
	vec3 normalVec = cross(faceVec1,faceVec2);
	if (dot(ray.getDirection(),normalVec) > 0){
		normalVec = -normalVec;
	}

	record.t = bvhHit.t;
	record.normalVec = normalVec;
	record.hitPointVec = ray.getPointAtT(record.t);
	record.material = nullptr;
	return true;
}

// New Mesh Construction
// Used to create boxes to avoid triangle recalcultaion

Mesh::Mesh(vector<vec3> & completeVerts, const vector<vec3> &faces):
	m_vertices(completeVerts),
	m_faces(){
	for (size_t i = 0; i < faces.size(); i++){
		m_faces.push_back( Triangle( (size_t) faces[i].x,
									 (size_t) faces[i].y,
									 (size_t) faces[i].z ) );

	}

	m_bvh.build(m_vertices, m_faces);
}
  
  
