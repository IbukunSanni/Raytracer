// Termm--Fall 2020

#pragma once

#include "scene/SceneNode.hpp"
#include "geometry/Primitive.hpp"
#include "scene/Material.hpp"

class GeometryNode : public SceneNode {
public:
	GeometryNode( const std::string & name, Primitive *prim, 
		Material *mat = nullptr );

	void setMaterial( Material *material );

	Material *m_material;
	Primitive *m_primitive;
	// TODO: define hit for geometry node
	virtual bool isHit(Ray & ray,float t0Float,float t1Float, HitRecord &record ) override;
  
};
