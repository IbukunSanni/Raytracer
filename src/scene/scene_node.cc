// Termm--Fall 2020

#include "scene/scene_node.h"

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <sstream>

#include "math/math_utils.h"
#include "scene/geometry_node.h"
#include "scene/material.h"

// Static class variable
unsigned int SceneNode::node_instance_count_ = 0;

//---------------------------------------------------------------------
// Initialiser order matches the declaration order in the header; members are
// constructed in declaration order regardless of what is written here, so a
// mismatched list only misleads the reader.
SceneNode::SceneNode(const std::string& name)
    : trans(glm::mat4()),
      invtrans(glm::mat4()),
      node_type(NodeType::kSceneNode),
      name(name),
      node_id(node_instance_count_++) {}

//---------------------------------------------------------------------
// Deep copy
SceneNode::SceneNode(const SceneNode& other)
    : trans(other.trans),
      invtrans(other.invtrans),
      node_type(other.node_type),
      name(other.name) {
  for (SceneNode* child : other.children) {
    this->children.push_front(new SceneNode(*child));
  }
}

//---------------------------------------------------------------------
SceneNode::~SceneNode() {
  for (SceneNode* child : children) {
    delete child;
  }
}

//---------------------------------------------------------------------
void SceneNode::SetTransform(const glm::mat4& m) {
  trans = m;
  invtrans = glm::inverse(m);
}

//---------------------------------------------------------------------
const glm::mat4& SceneNode::GetTransform() const { return trans; }

//---------------------------------------------------------------------
const glm::mat4& SceneNode::GetInverse() const { return invtrans; }

//---------------------------------------------------------------------
void SceneNode::AddChild(SceneNode* child) { children.push_back(child); }

//---------------------------------------------------------------------
void SceneNode::RemoveChild(SceneNode* child) { children.remove(child); }

//---------------------------------------------------------------------
void SceneNode::Rotate(char axis, float angle) {
  glm::vec3 rot_axis;

  switch (axis) {
    case 'x':
      rot_axis = glm::vec3(1, 0, 0);
      break;
    case 'y':
      rot_axis = glm::vec3(0, 1, 0);
      break;
    case 'z':
      rot_axis = glm::vec3(0, 0, 1);
      break;
    default:
      break;
  }
  glm::mat4 rot_matrix = glm::rotate(DegreesToRadians(angle), rot_axis);
  SetTransform(rot_matrix * trans);
}

//---------------------------------------------------------------------
void SceneNode::Scale(const glm::vec3& amount) {
  SetTransform(glm::scale(amount) * trans);
}

//---------------------------------------------------------------------
void SceneNode::Translate(const glm::vec3& amount) {
  SetTransform(glm::translate(amount) * trans);
}

//---------------------------------------------------------------------
int SceneNode::TotalSceneNodes() const { return node_instance_count_; }

//---------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const SceneNode& node) {
  // os << "SceneNode:[NodeType: ___, name: ____, id: ____, is_selected: ____,
  // transform: ____"
  switch (node.node_type) {
    case NodeType::kSceneNode:
      os << "SceneNode";
      break;
    case NodeType::kGeometryNode:
      os << "GeometryNode";
      break;
    case NodeType::kJointNode:
      os << "JointNode";
      break;
  }
  os << ":[";

  os << "name:" << node.name << ", ";
  os << "id:" << node.node_id;

  // No trailing newline: a streaming operator should not decide where lines
  // break. The caller does -- and a log record must stay on one line.
  os << "]";
  return os;
}

//---------------------------------------------------------------------
Ray SceneNode::ToLocal(Ray& ray) const {
  return Ray(glm::vec3(GetInverse() * glm::vec4(ray.GetOrigin(), 1.0f)),
             glm::vec3(GetInverse() * glm::vec4(ray.GetDirection(), 0.0f)),
             ray.GetTime(), ray.GetRng());
}

//---------------------------------------------------------------------
void SceneNode::ToWorld(HitRecord& hit) const {
  hit.SetNormal(glm::mat3(transpose(GetInverse())) * hit.GetNormal());
  hit.SetHitPoint(
      glm::vec3(GetTransform() * glm::vec4(hit.GetHitPoint(), 1.0f)));
}

//---------------------------------------------------------------------
AABB SceneNode::ChildrenBounds() const {
  AABB box;
  for (const SceneNode* child : children) box.Expand(child->Bounds());
  return box;
}

//---------------------------------------------------------------------
AABB SceneNode::Bounds() const { return ChildrenBounds().Transformed(trans); }
//---------------------------------------------------------------------

void SceneNode::CacheBounds() {
  cached_bounds_ = Bounds();
  for (SceneNode* child : children) child->CacheBounds();
}

//---------------------------------------------------------------------
bool SceneNode::IsHit(Ray& ray, float t0_float, float t1_float,
                      HitRecord& hit) {
  Ray local_ray = ToLocal(ray);

  HitRecord local_hit;
  bool is_hit = HitChildren(local_ray, t0_float, t1_float, local_hit);

  if (is_hit) {
    hit = local_hit;
    ToWorld(hit);
  }

  return is_hit;
}

//---------------------------------------------------------------------
bool SceneNode::HitChildren(Ray& local_ray, float t0_float, float t1_float,
                            HitRecord& hit) {
  bool is_hit = false;
  // Computed once so each child's slab test only multiplies.
  const glm::vec3 inv_dir_vec = 1.0f / local_ray.GetDirection();

  for (SceneNode* child : children) {
    // Skip children whose box the ray misses; infinite boxes always get tested.
    const AABB& box = child->cached_bounds_;
    if (box.IsFinite() &&
        !box.Hit(local_ray.GetOrigin(), inv_dir_vec, t0_float, t1_float)) {
      continue;
    }

    HitRecord child_hit;
    // Each child transforms the ray in its own IsHit. The material is set by
    // the GeometryNode that owns the hit, so a parent never clobbers it here.
    if (child->IsHit(local_ray, t0_float, t1_float, child_hit)) {
      is_hit = true;
      // Narrow the search so the nearest hit wins.
      t1_float = child_hit.GetT();
      hit = child_hit;
    }
  }

  return is_hit;
}
