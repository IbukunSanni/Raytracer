// Termm--Fall 2020

#include "SceneNode.hpp"

#include "cs488-framework/MathUtils.hpp"

#include <iostream>
#include <sstream>
using namespace std;

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/transform.hpp>

using namespace glm;
#include "GeometryNode.hpp"
#include "Material.hpp"


// Static class variable
unsigned int SceneNode::nodeInstanceCount = 0;


//---------------------------------------------------------------------------------------
SceneNode::SceneNode(const std::string& name)
  : m_name(name),
	m_nodeType(NodeType::SceneNode),
	trans(mat4()),
	invtrans(mat4()),
	m_nodeId(nodeInstanceCount++)
{

}

//---------------------------------------------------------------------------------------
// Deep copy
SceneNode::SceneNode(const SceneNode & other)
	: m_nodeType(other.m_nodeType),
	  m_name(other.m_name),
	  trans(other.trans),
	  invtrans(other.invtrans)
{
	for(SceneNode * child : other.children) {
		this->children.push_front(new SceneNode(*child));
	}
}

//---------------------------------------------------------------------------------------
SceneNode::~SceneNode() {
	for(SceneNode * child : children) {
		delete child;
	}
}

//---------------------------------------------------------------------------------------
void SceneNode::set_transform(const glm::mat4& m) {
	trans = m;
	invtrans = glm::inverse(m);
}

//---------------------------------------------------------------------------------------
const glm::mat4& SceneNode::get_transform() const {
	return trans;
}

//---------------------------------------------------------------------------------------
const glm::mat4& SceneNode::get_inverse() const {
	return invtrans;
}

//---------------------------------------------------------------------------------------
void SceneNode::add_child(SceneNode* child) {
	children.push_back(child);
}

//---------------------------------------------------------------------------------------
void SceneNode::remove_child(SceneNode* child) {
	children.remove(child);
}

//---------------------------------------------------------------------------------------
void SceneNode::rotate(char axis, float angle) {
	vec3 rot_axis;

	switch (axis) {
		case 'x':
			rot_axis = vec3(1,0,0);
			break;
		case 'y':
			rot_axis = vec3(0,1,0);
	        break;
		case 'z':
			rot_axis = vec3(0,0,1);
	        break;
		default:
			break;
	}
	mat4 rot_matrix = glm::rotate(degreesToRadians(angle), rot_axis);
	set_transform( rot_matrix * trans );
}

//---------------------------------------------------------------------------------------
void SceneNode::scale(const glm::vec3 & amount) {
	set_transform( glm::scale(amount) * trans );
}

//---------------------------------------------------------------------------------------
void SceneNode::translate(const glm::vec3& amount) {
	set_transform( glm::translate(amount) * trans );
}


//---------------------------------------------------------------------------------------
int SceneNode::totalSceneNodes() const {
	return nodeInstanceCount;
}

//---------------------------------------------------------------------------------------
std::ostream & operator << (std::ostream & os, const SceneNode & node) {

	//os << "SceneNode:[NodeType: ___, name: ____, id: ____, isSelected: ____, transform: ____"
	switch (node.m_nodeType) {
		case NodeType::SceneNode:
			os << "SceneNode";
			break;
		case NodeType::GeometryNode:
			os << "GeometryNode";
			break;
		case NodeType::JointNode:
			os << "JointNode";
			break;
	}
	os << ":[";

	os << "name:" << node.m_name << ", ";
	os << "id:" << node.m_nodeId;

	os << "]\n";
	return os;
}

bool SceneNode::isHit(RayTracer & ray,float t0Float,float t1Float, HitRecord &record ){
	
	// Get Local Transformation
	RayTracer localRay;
	const vec3 localRayOriginVec = vec3(get_inverse() *vec4(ray.getOrigin(),1.0f));
	localRay.setOrigin(localRayOriginVec);
	const vec3 localRayDirVec = vec3(get_inverse() * vec4(ray.getDirection(),0.0f));
	localRay.setDirection(localRayDirVec);

	HitRecord localRecord;
	bool hit  = false;
	
	// Traverse each child of the root and check if the ray hits
	for (SceneNode* child : children){
		bool checkHit = false;
		if(child->m_nodeType == NodeType::GeometryNode){
			const GeometryNode * geometryNode = static_cast<const GeometryNode *>(child);
			checkHit = geometryNode->m_primitive->isHit(localRay,t0Float,t1Float, localRecord);
			// Update record material for use in rendering
			localRecord.material = geometryNode->m_material;
		}else{
			checkHit = child->isHit(localRay,t0Float,t1Float, localRecord);
		}

		if (checkHit){
			hit = true;
			// Max t updated to find element closest to screen
			t1Float = localRecord.t; 
			// Updated Max t and record Material updating record
			record = localRecord;
		}
	}
	
	if (hit){
		// Restore transformation
		record.normalVec = mat3(transpose(get_inverse())) * record.normalVec;
		record.hitPointVec = vec3(get_transform() * vec4(record.hitPointVec,1.0f));
	}
	
	return hit;
	
}