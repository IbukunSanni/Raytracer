// Termm--Fall 2020

#pragma once

#include "SceneNode.hpp"
#include "Primitive.hpp"
#include "Material.hpp"

class GeometryNode : public SceneNode {
public:
	GeometryNode( const std::string & name, Primitive *prim, 
		Material *mat = nullptr );

	void setMaterial( Material *material );

	Material *m_material;
	Primitive *m_primitive;
	// TODO: define hit for geometry node
	// virtual bool isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) override;
  
};
