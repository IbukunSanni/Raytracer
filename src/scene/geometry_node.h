// Termm--Fall 2020

#ifndef RAYTRACER_SRC_SCENE_GEOMETRY_NODE_H_
#define RAYTRACER_SRC_SCENE_GEOMETRY_NODE_H_

#include "geometry/primitive.h"
#include "scene/material.h"
#include "scene/scene_node.h"

class GeometryNode : public SceneNode {
 public:
  GeometryNode(const std::string& name, Primitive* prim,
               Material* mat = nullptr);

  void SetMaterial(Material* material);

  Material* material;
  Primitive* primitive;
  // TODO: define hit for geometry node
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;
};

#endif  // RAYTRACER_SRC_SCENE_GEOMETRY_NODE_H_
