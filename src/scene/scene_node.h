
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

  // True when this node's children are currently searched through a tree.
  // For tests.
  bool HasChildTree() const { return tree_built_; }

 protected:
  // Ray transport helpers. The transform must be applied exactly ONCE per
  // node: ToLocal on the way in, ToWorld on the way out. Applying it twice
  // displaces everything below the node, so every node type goes through
  // these.
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
  // A tree only pays when culling can skip whole groups of children; on a
  // node with a handful of children, its interior boxes cost more than they
  // save. 16 is a first guess, not tuned.
  static constexpr size_t kMinTreeChildren = 16;

  // One node of the tree over this node's finite-bounds children, in this
  // node's own space (the space a ray is in once this node has transformed
  // it). A leaf holds one child; an interior node holds two subtrees and no
  // child.
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

  // HitChildren for a node with a tree: the unbounded children linearly,
  // then the tree.
  bool HitChildrenByTree(Ray& local_ray, float t0_float, float t1_float,
                         HitRecord& hit);

  // The number of SceneNode instances.
  static unsigned int node_instance_count_;
  AABB cached_bounds_ = AABB::Infinite();
  // Built once before rendering starts, and only read while threads render.
  // Not copied: a copy starts without one.
  std::unique_ptr<ChildTreeNode> child_tree_;
  // Children with infinite bounds (planes) can't go in a tree; tested linearly.
  std::vector<SceneNode*> unbounded_children_;
  // False whenever there is no current tree: too few children, bounds not
  // cached yet, or the child list changed since. The children are then
  // scanned linearly, culling on their boxes.
  bool tree_built_ = false;
  // True when trans is exactly the identity, so a ray and its hit pass
  // through this node untouched. Must change wherever trans changes.
  bool is_identity_;
};

#endif  // RAYTRACER_SRC_SCENE_SCENE_NODE_H_
