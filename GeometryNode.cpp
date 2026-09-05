// Termm--Fall 2020

#include "GeometryNode.hpp"

//---------------------------------------------------------------------------------------
GeometryNode::GeometryNode(
	const std::string & name, Primitive *prim, Material *mat )
	: SceneNode( name )
	, m_material( mat )
	, m_primitive( prim )
{
	m_nodeType = NodeType::GeometryNode;
}

void GeometryNode::setMaterial( Material *mat )
{
	// Obviously, there's a potential memory leak here.  A good solution
	// would be to use some kind of reference counting, as in the 
	// C++ shared_ptr.  But I'm going to punt on that problem here.
	// Why?  Two reasons:
	// (a) In practice we expect the scene to be constructed exactly
	//     once.  There's no reason to believe that materials will be
	//     repeatedly overwritten in a GeometryNode.
	// (b) A ray tracer is a program in which you compute once, and 
	//     throw away all your data.  A memory leak won't build up and
	//     crash the program.

	m_material = mat;
}

bool GeometryNode::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ) {
	// Transform into this node's space exactly once.
	RayTracer localRay = toLocal(ray);

	bool hit = false;

	// This node's own primitive.
	HitRecord primRecord;
	if (m_primitive->isHit(localRay, t0Float, t1Float, primRecord)){
		primRecord.material = m_material;
		hit = true;
		t1Float = primRecord.t;   // narrow the search
		record = primRecord;
	}

	// Any children. hitChildren does NOT re-transform -- localRay is already
	// in this node's space, and each child applies its own transform.
	HitRecord childRecord;
	if (hitChildren(localRay, t0Float, t1Float, childRecord)){
		hit = true;
		t1Float = childRecord.t;
		record = childRecord;
	}

	if (hit){
		toWorld(record);
	}

	return hit;
}
