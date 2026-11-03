
#ifndef RAYTRACER_SRC_SCENE_SCENE_NODE_H_
#define RAYTRACER_SRC_SCENE_SCENE_NODE_H_

#include <glm/glm.hpp>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/aabb.h"
#include "scene/material.h"

enum class NodeType { kSceneNode, kGeometryNode, kJointNode };

class SceneNode {
 public:
  explicit SceneNode(const std::string& name);

  SceneNode(const SceneNode& other);

  virtual ~SceneNode();

  int TotalSceneNodes() const;

  const glm::mat4& GetTransform() const;
  const glm::mat4& GetInverse() const;

  void SetTransform(const glm::mat4& m);

  void AddChild(SceneNode* child);

  void RemoveChild(SceneNode* child);

  //-- Transformations:
  void Rotate(char axis, float angle);
  void Scale(const glm::vec3& amount);
  void Translate(const glm::vec3& amount);

  friend std::ostream& operator<<(std::ostream& os, const SceneNode& node);

  // Transformations
  glm::mat4 trans;
  glm::mat4 invtrans;

  std::list<SceneNode*> children;

  NodeType node_type;
  std::string name;
  unsigned int node_id;

  virtual bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit);

  // A box holding every hit on this node and below, in its parent's space:
  // the space a ray is in when it reaches this node's IsHit.
  virtual AABB Bounds() const;

  void CacheBounds();

  // True when CacheBounds() built a tree over this node's children, which
  // it does only for kMinTreeChildren or more. For tests.
  bool HasChildTree() const { return tree_built_; }

 protected:
  // Ray transport helpers. The transform must be applied exactly ONCE per
  // node: ToLocal on the way in, ToWorld on the way out. GeometryNode used
  // to transform and then delegate to SceneNode::IsHit, which transformed
  // again with the same matrix -- so anything parented to a GeometryNode
  // was displaced. These exist so both node types share one code path.
  Ray ToLocal(Ray& ray) const;
  void ToWorld(HitRecord& hit) const;

  // Intersect this node's children with a ray ALREADY in local space.
  // Does not transform: the caller has done it, and each child applies
  // its own transform inside its own IsHit.
  bool HitChildren(Ray& local_ray, float t0_float, float t1_float,
                   HitRecord& hit);

  // The union of the children's bounds, in this node's own space.
  AABB ChildrenBounds() const;

 private:
  // A tree only pays when culling can skip whole groups of children. With a
  // tree on every node, cornell_smoke (6 to 8 children per node) ran 0.88x:
  // the interior boxes cost more than they saved. 16 is a first guess, not
  // tuned yet.
  static constexpr size_t kMinTreeChildren = 16;

  // One node of the tree over this node's finite-bounds children, in this
  // node's own space (the space HitChildren's ray is in). A leaf holds one
  // child; an interior node holds two subtrees and no child.
  struct ChildTreeNode {
    AABB box;
    SceneNode* child = nullptr;  // leaf only
    std::unique_ptr<ChildTreeNode> left;
    std::unique_ptr<ChildTreeNode> right;
  };

  // Median split on the longest axis over list[begin, end), which must not
  // be empty. Reorders that range.
  static std::unique_ptr<ChildTreeNode> BuildChildTree(
      std::vector<SceneNode*>& list, size_t begin, size_t end);

  // Closest hit among the children under `node`. Narrows t1_float as hits
  // are found, so a later subtree is culled by an earlier subtree's hit.
  static bool HitChildTree(const ChildTreeNode& node, Ray& local_ray,
                           const glm::vec3& inv_dir_vec, float t0_float,
                           float& t1_float, HitRecord& hit);

  // The number of SceneNode instances.
  static unsigned int node_instance_count_;
  AABB cached_bounds_ = AABB::Infinite();
  // Built by CacheBounds(), before the render threads start; read-only after.
  // Not copied: a copy starts empty and CacheBounds() rebuilds it.
  std::unique_ptr<ChildTreeNode> child_tree_;
  // Children with infinite bounds (planes) can't go in a tree; tested linearly.
  std::vector<SceneNode*> unbounded_children_;
  // False until CacheBounds() builds a tree, for nodes with fewer than
  // kMinTreeChildren children, and again after AddChild/RemoveChild;
  // HitChildren then scans the children linearly, culling on their boxes.
  bool tree_built_ = false;
  // True when trans is exactly the identity, so ToLocal/ToWorld can return
  // the ray and hit untouched. Set wherever trans is: the constructors and
  // SetTransform. Nothing else writes trans (checked 2 November); if
  // something starts to, it must go through SetTransform.
  bool is_identity_;
};

#endif  // RAYTRACER_SRC_SCENE_SCENE_NODE_H_
