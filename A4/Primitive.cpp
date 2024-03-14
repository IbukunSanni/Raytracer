// Termm--Fall 2020

#include "Primitive.hpp"

#include "polyroots.hpp"
#include "RayTracer.hpp"
#include "Mesh.hpp"

#include <iostream>
#include <sstream>
#include <vector>
using namespace glm;
using namespace std;

Primitive::~Primitive()
{
}

// If primitive is not defined
// default is no hit
bool Primitive::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
    return false;
 }

Sphere::~Sphere()
{
}
bool Sphere::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
    auto nh_sphere = new NonhierSphere(vec3(0.0,0.0,0.0),1.0);
    return nh_sphere->isHit(ray,t0Float,t1Float,record );

}

Cube::~Cube()
{
}

bool Cube::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
    auto nh_cube = new NonhierBox(vec3(0.0,0.0,0.0),1.0);
    return nh_cube->isHit(ray,t0Float,t1Float,record );
}

NonhierSphere::~NonhierSphere()
{
}

// Use quadractic roots to calculate if a sphere is hit
bool NonhierSphere::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
    vec3 eMinusCVec = ray.getOrigin() - m_pos;
    vec3 dVec = ray.getDirection();

    double A = (double) dot(dVec,dVec);
    double B = (double) (2 * dot(dVec,eMinusCVec));
    double C = (double) (dot(eMinusCVec,eMinusCVec) - (m_radius *m_radius));

    double roots[2];
    size_t  numRoots = quadraticRoots(A,B,C,roots);

    float tFloat= 0;
    switch (numRoots){
        case 0:
            return false;
        case 1:
            tFloat = (float)roots[0];
            break;
        default:// case 2
            tFloat =(float) glm::min(roots[0],roots[1]);
            break;
    }

    if (tFloat <= t0Float || t1Float <= tFloat ){
        return false;
    }

    record.t = tFloat;
    record.hitPointVec = ray.getPointAtT(tFloat);
    record.normalVec = record.hitPointVec - m_pos;
    return true;

}

NonhierBox::~NonhierBox()
{
}

bool NonhierBox::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
    // cout << "NonhierBox::isHit() called" << endl;
    // Create vertices
    std::vector<glm::vec3> vertices(8);
    // Bottom face
    vertices[0] = m_pos + vec3(0.0f,0.0f,0.0f);
    vertices[1] = m_pos + vec3(m_size,0.0f,0.0f);
    vertices[2] = m_pos + vec3(m_size,0.0f,m_size);
    vertices[3] = m_pos + vec3(0.0f,0.0f,m_size);
    // Top face
    vertices[4] = m_pos + vec3(0.0f,m_size,0.0f);
    vertices[5] = m_pos + vec3(m_size,m_size,0.0f);
    vertices[6] = m_pos + vec3(m_size,m_size,m_size);
    vertices[7] = m_pos + vec3(0.0f,m_size,m_size);

    vector<vec3> tri_idx = {
        vec3(0,1,2),
        vec3(0,2,3),
        vec3(0,7,4),
        vec3(0,3,7),
        vec3(0,4,5),
        vec3(0,5,1),

        vec3(6,2,1),
        vec3(6,1,5),
        vec3(6,5,4),
        vec3(6,4,7),
        vec3(6,7,3),
        vec3(6,3,2)
    };

    m_mesh = new Mesh(vertices,tri_idx);
    bool res = m_mesh->isHit(ray,t0Float,t1Float,record );
    // cout << "NonhierBox::isHit() left" << endl;
    return res;
}
