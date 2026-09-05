// Termm--Fall 2020

#include "scene/SceneNode.hpp"

#include "math/MathUtils.hpp"

#include <iostream>
#include <sstream>
using namespace std;

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/transform.hpp>

using namespace glm;
#include "scene/GeometryNode.hpp"
#include "scene/Material.hpp"


// Static class variable
unsigned int SceneNode::nodeInstanceCount = 0;


//---------------------------------------------------------------------------------------
// Initialiser order matches the declaration order in the header; members are
// constructed in declaration order regardless of what is written here, so a
// mismatched list only misleads the reader.
SceneNode::SceneNode(const std::string& name)
  : trans(mat4()),
	invtrans(mat4()),
	m_nodeType(NodeType::SceneNode),
	m_name(name),
	m_nodeId(nodeInstanceCount++)
{

}

//---------------------------------------------------------------------------------------
// Deep copy
SceneNode::SceneNode(const SceneNode & other)
	: trans(other.trans),
	  invtrans(other.invtrans),
	  m_nodeType(other.m_nodeType),
	  m_name(other.m_name)
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

	// No trailing newline: a streaming operator should not decide where lines
	// break. The caller does -- and a log record must stay on one line.
	os << "]";
	return os;
}

//---------------------------------------------------------------------------------------
Ray SceneNode::toLocal(Ray & ray) const {
	Ray localRay;
	localRay.setOrigin( vec3(get_inverse() * vec4(ray.getOrigin(), 1.0f)) );
	localRay.setDirection( vec3(get_inverse() * vec4(ray.getDirection(), 0.0f)) );
	return localRay;
}

//---------------------------------------------------------------------------------------
void SceneNode::toWorld(HitRecord & record) const {
	record.normalVec = mat3(transpose(get_inverse())) * record.normalVec;
	record.hitPointVec = vec3(get_transform() * vec4(record.hitPointVec, 1.0f));
}

//---------------------------------------------------------------------------------------
bool SceneNode::hitChildren(Ray & localRay,float t0Float,float t1Float, HitRecord &record ){
	bool hit = false;

	for (SceneNode* child : children){
		HitRecord childRecord;
		// Each child applies its own transform inside its own isHit, so the
		// ray is passed through unchanged. The material is set by whichever
		// GeometryNode actually owns the primitive that was hit -- setting it
		// here would clobber a nested child's material with the parent's.
		if (child->isHit(localRay, t0Float, t1Float, childRecord)){
			hit = true;
			// Narrow the search so the nearest hit wins.
			t1Float = childRecord.t;
			record = childRecord;
		}
	}

	return hit;
}

//---------------------------------------------------------------------------------------
bool SceneNode::isHit(Ray & ray,float t0Float,float t1Float, HitRecord &record ){
	Ray localRay = toLocal(ray);

	HitRecord localRecord;
	bool hit = hitChildren(localRay, t0Float, t1Float, localRecord);

	if (hit){
		record = localRecord;
		toWorld(record);
	}

	return hit;
}
