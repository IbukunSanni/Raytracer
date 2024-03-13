// Termm--Fall 2020

#pragma once

#include <vector>
#include <iosfwd>
#include <string>

#include <glm/glm.hpp>

#include "Primitive.hpp"

// Use this #define to selectively compile your code to render the
// bounding boxes around your mesh objects. Uncomment this option
// to turn it on.
//#define RENDER_BOUNDING_VOLUMES

struct Triangle
{
	size_t v1;
	size_t v2;
	size_t v3;

	Triangle( size_t pv1, size_t pv2, size_t pv3 )
		: v1( pv1 )
		, v2( pv2 )
		, v3( pv3 )
	{}
};

// A polygonal mesh.
class Mesh : public Primitive {
public:
  Mesh( const std::string& fname );
  Mesh(vector<vec3> & completeVerts, const vector<vec3> &faces);
  bool isTriangleIntersection(RayTracer &ray,vec3 vert0, vec3 vert1, vec3 vert2, float &potT1Float,float t0Float,float t1Float);
  virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) override;
  
  
private:
	std::vector<glm::vec3> m_vertices;
	std::vector<Triangle> m_faces;

    friend std::ostream& operator<<(std::ostream& out, const Mesh& mesh);
};
