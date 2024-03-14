// Termm--Fall 2020

#pragma once

#include <glm/glm.hpp>

#include "RayTracer.hpp"
#include "HitRecord.hpp"

using namespace glm;
using namespace std;

class Primitive {
public:
  virtual ~Primitive();
  virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record );
};

class Sphere : public Primitive {
public:
  virtual ~Sphere();
  virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) override;
};

class Cube : public Primitive {
public:
  virtual ~Cube();
  virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) override;
};

class NonhierSphere : public Primitive {
public:
  NonhierSphere(const glm::vec3& pos, double radius)
    : m_pos(pos), m_radius(radius)
  {
  }
  virtual ~NonhierSphere();
  virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) override;


private:
  glm::vec3 m_pos;
  double m_radius;
};

class NonhierBox : public Primitive {
public:
  NonhierBox(const glm::vec3& pos, double size)
    : m_pos(pos), m_size(size)
  {
  }
  
  virtual ~NonhierBox();
  virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) override;
  
private:
  glm::vec3 m_pos;
  double m_size;

  Primitive * m_mesh;
};
