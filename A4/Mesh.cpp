// Termm--Fall 2020

#include <iostream>
#include <fstream>

#include <glm/ext.hpp>

// #include "cs488-framework/ObjFileDecoder.hpp"
#include "Mesh.hpp"

using namespace glm;
using namespace std;
// TODO: confirm const is good
static const float EPS = 0.00001;

Mesh::Mesh( const std::string& fname )
	: m_vertices()
	, m_faces()
{
	cout << "initialized mesh Mesh(fname) entered"<<endl;
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

	cout << "initialized mesh Mesh(fname) exited"<<endl;
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

bool Mesh::isTriangleIntersection(RayTracer &ray,vec3 vert0, vec3 vert1, vec3 vert2,float &potT1Float,float t0Float,float t1Float){
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
	if(gamma< EPS or gamma > 1){
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


bool Mesh::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
	// cout << "Mesh::isHit() called" << endl;
	bool hit = false;
	vec3 normalVec = vec3();
	float newT1float = t1Float;
	// Traverse faces to see a hit
	for (auto face: m_faces){
		float potT1Float = 0.0f;
		// Check for intersection with face
		if (isTriangleIntersection(ray, m_vertices[face.v1], m_vertices[face.v2], m_vertices[face.v3], potT1Float,t0Float,t1Float)){
			// TODO: Print potT1Float, be sure it is changing
			// cout<< "potT1float = " << potT1Float<< endl;
			hit = true;
			newT1float = potT1Float;
			// TODO: Check orientation to avoid flipping normals
			// Can correct by switching cross position
			vec3 faceVec1 = m_vertices[face.v1] - m_vertices[face.v2];
			vec3 faceVec2 = m_vertices[face.v2] - m_vertices[face.v3];
			normalVec = cross(faceVec1,faceVec2);
		}

	}
	if (!hit){
		// cout << "Mesh::isHit() left false" << endl;
		return false;
	}

	if (dot(ray.getDirection(),normalVec)> 0){
		normalVec = -normalVec;
	}

	// Update Record
	record.t = newT1float;
	record.normalVec = normalVec;
	record.hitPointVec = ray.getPointAtT(record.t);
	record.material = nullptr;

	// cout << "Mesh::isHit() left false" << endl;
	return hit;

}

// New Mesh Construction
// Used to create boxes to avoid triangle recalcultaion

Mesh::Mesh(vector<vec3> & completeVerts, const vector<vec3> &faces):
	m_vertices(completeVerts),
	m_faces(){
	for (int i =0; i < faces.size(); i++){
		m_faces.push_back( Triangle( (size_t) faces[i].x,
									 (size_t) faces[i].y,
									 (size_t) faces[i].z ) );

	}

}
  
  
