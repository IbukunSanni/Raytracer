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
	// Get Local Transformation
	RayTracer localRay;
	const vec3 localRayOriginVec = vec3(get_inverse() *vec4(ray.getOrigin(),1.0f));
	localRay.setOrigin(localRayOriginVec);
	const vec3 localRayDirVec = vec3(get_inverse() * vec4(ray.getDirection(),0.0f));
	localRay.setDirection(localRayDirVec);

	HitRecord localRecord;
	localRecord.material = NULL;
	bool hit  = false;
	
	// Check if mesh attribute hits and update closest position
	if(m_primitive->isHit(localRay,t0Float,t1Float, localRecord)){
		localRecord.material = m_material;
		hit = true;
		// Max t updated to find element closest to screen
		t1Float = localRecord.t; 
		// Updated Max t and record Material updating record
		record = localRecord;
	}

	// recursive  call
	if (SceneNode::isHit(localRay,t0Float,t1Float, localRecord)){
		hit = true;
		// Max t updated to find element closest to screen
		t1Float = localRecord.t; 
		// Updated Max t and record Material updating record
		record = localRecord;
	}

	if (hit){
		// Restore transformation
		record.normalVec = mat3(transpose(get_inverse())) * record.normalVec;
		record.hitPointVec = vec3(get_transform() * vec4(record.hitPointVec,1.0f));
	}
	
	return hit;
}
