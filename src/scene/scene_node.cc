// Termm--Fall 2020

#include "scene/scene_node.h"

#include <algorithm>
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
      node_id(node_instance_count_++),
      is_identity_(trans == glm::mat4(1.0f)) {}

//---------------------------------------------------------------------
// Deep copy
SceneNode::SceneNode(const SceneNode& other)
    : trans(other.trans),
      invtrans(other.invtrans),
      node_type(other.node_type),
      name(other.name),
      is_identity_(other.is_identity_) {
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
  is_identity_ = (m == glm::mat4(1.0f));
}

//---------------------------------------------------------------------
const glm::mat4& SceneNode::GetTransform() const { return trans; }

//---------------------------------------------------------------------
const glm::mat4& SceneNode::GetInverse() const { return invtrans; }

//---------------------------------------------------------------------
// Both drop the tree, so a changed child list is never traversed through a
// stale one. Neither may run while the render threads are reading it.
void SceneNode::AddChild(SceneNode* child) {
  children.push_back(child);
  tree_built_ = false;
}

//---------------------------------------------------------------------
void SceneNode::RemoveChild(SceneNode* child) {
  children.remove(child);
  tree_built_ = false;
}

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
  if (is_identity_) return ray;
  return Ray(glm::vec3(GetInverse() * glm::vec4(ray.GetOrigin(), 1.0f)),
             glm::vec3(GetInverse() * glm::vec4(ray.GetDirection(), 0.0f)),
             ray.GetTime(), ray.GetRng());
}

//---------------------------------------------------------------------
void SceneNode::ToWorld(HitRecord& hit) const {
  if (is_identity_) return;
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

  unbounded_children_.clear();
  child_tree_.reset();
  tree_built_ = false;
  if (children.size() < kMinTreeChildren) return;

  // The children have cached their boxes above, so the tree is built from them.
  std::vector<SceneNode*> bounded;
  for (SceneNode* child : children) {
    if (child->cached_bounds_.IsFinite()) {
      bounded.push_back(child);
    } else {
      unbounded_children_.push_back(child);
    }
  }
  child_tree_ =
      bounded.empty() ? nullptr : BuildChildTree(bounded, 0, bounded.size());
  tree_built_ = true;
}

//---------------------------------------------------------------------
std::unique_ptr<SceneNode::ChildTreeNode> SceneNode::BuildChildTree(
    std::vector<SceneNode*>& list, size_t begin, size_t end) {
  auto node = std::make_unique<ChildTreeNode>();
  for (size_t i = begin; i < end; ++i) {
    node->box.Expand(list[i]->cached_bounds_);
  }

  if (end - begin == 1) {
    node->child = list[begin];
    return node;
  }

  // A full sort is the simplest correct split; nth_element (as in the mesh
  // BVH) is enough, but build time is outside the render timer.
  const int axis = node->box.LongestAxis();
  std::sort(list.begin() + begin, list.begin() + end,
            [axis](const SceneNode* a, const SceneNode* b) {
              return a->cached_bounds_.Centroid()[axis] <
                     b->cached_bounds_.Centroid()[axis];
            });

  const size_t mid = begin + (end - begin) / 2;
  node->left = BuildChildTree(list, begin, mid);
  node->right = BuildChildTree(list, mid, end);
  return node;
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
  // Computed once so each box's slab test only multiplies.
  const glm::vec3 inv_dir_vec = 1.0f / local_ray.GetDirection();

  auto test_child = [&](SceneNode* child) {
    HitRecord child_hit;
    // Each child transforms the ray in its own IsHit. The material is set by
    // the GeometryNode that owns the hit, so a parent never clobbers it here.
    if (child->IsHit(local_ray, t0_float, t1_float, child_hit)) {
      is_hit = true;
      // Narrow the search so the nearest hit wins.
      t1_float = child_hit.GetT();
      hit = child_hit;
    }
  };

  // No tree (too few children, or CacheBounds() not called since the
  // children last changed): rung 2's scan. Skip children whose box the ray
  // misses; infinite boxes, which include any child not cached yet, always
  // get tested.
  if (!tree_built_) {
    for (SceneNode* child : children) {
      const AABB& box = child->cached_bounds_;
      if (box.IsFinite() &&
          !box.Hit(local_ray.GetOrigin(), inv_dir_vec, t0_float, t1_float)) {
        continue;
      }
      test_child(child);
    }
    return is_hit;
  }

  // Infinite boxes can't be culled, so these are always tested.
  for (SceneNode* child : unbounded_children_) test_child(child);

  if (child_tree_ && HitChildTree(*child_tree_, local_ray, inv_dir_vec,
                                  t0_float, t1_float, hit)) {
    is_hit = true;
  }

  return is_hit;
}

//---------------------------------------------------------------------
bool SceneNode::HitChildTree(const ChildTreeNode& node, Ray& local_ray,
                             const glm::vec3& inv_dir_vec, float t0_float,
                             float& t1_float, HitRecord& hit) {
  // A miss here skips every child below, which is the whole point of the tree.
  if (!node.box.Hit(local_ray.GetOrigin(), inv_dir_vec, t0_float, t1_float)) {
    return false;
  }

  if (node.child != nullptr) {
    HitRecord child_hit;
    if (!node.child->IsHit(local_ray, t0_float, t1_float, child_hit)) {
      return false;
    }
    t1_float = child_hit.GetT();
    hit = child_hit;
    return true;
  }

  // Both always run (no ||, which would skip the right on a left hit), and
  // the right sees the t1_float the left narrowed. Left first, whatever the
  // ray direction: visiting the nearer child first is a later rung.
  const bool hit_left =
      HitChildTree(*node.left, local_ray, inv_dir_vec, t0_float, t1_float, hit);
  const bool hit_right = HitChildTree(*node.right, local_ray, inv_dir_vec,
                                      t0_float, t1_float, hit);
  return hit_left || hit_right;
}
