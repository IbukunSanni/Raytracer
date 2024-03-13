// Termm--Fall 2020

#include "Primitive.hpp"

#include "polyroots.hpp"
#include "RayTracer.hpp"

#include <iostream>
#include <sstream>
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

Cube::~Cube()
{
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
