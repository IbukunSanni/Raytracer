// Termm--Fall 2020

#ifndef RAYTRACER_SRC_SCENE_SCENE_NODE_H_
#define RAYTRACER_SRC_SCENE_SCENE_NODE_H_

#include <glm/glm.hpp>
#include <iostream>
#include <list>
#include <string>

#include "core/hit_record.h"
#include "core/ray.h"
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

  virtual bool IsHit(Ray& ray, float t0_float, float t1_float,
                     HitRecord& record);

 protected:
  // Ray transport helpers. The transform must be applied exactly ONCE per
  // node: ToLocal on the way in, ToWorld on the way out. GeometryNode used
  // to transform and then delegate to SceneNode::IsHit, which transformed
  // again with the same matrix -- so anything parented to a GeometryNode
  // was displaced. These exist so both node types share one code path.
  Ray ToLocal(Ray& ray) const;
  void ToWorld(HitRecord& record) const;

  // Intersect this node's children with a ray ALREADY in local space.
  // Does not transform: the caller has done it, and each child applies
  // its own transform inside its own IsHit.
  bool HitChildren(Ray& local_ray, float t0_float, float t1_float,
                   HitRecord& record);

 private:
  // The number of SceneNode instances.
  static unsigned int node_instance_count;
};

#endif  // RAYTRACER_SRC_SCENE_SCENE_NODE_H_
