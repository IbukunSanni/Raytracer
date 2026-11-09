#ifndef RAYTRACER_SRC_SCENE_GEOMETRY_NODE_H_
#define RAYTRACER_SRC_SCENE_GEOMETRY_NODE_H_

#include <memory>

#include "geometry/primitive.h"
#include "scene/material.h"
#include "scene/scene_node.h"
#include "scene/texture.h"

class GeometryNode : public SceneNode {
 public:
  GeometryNode(const std::string& name, Primitive* prim,
               Material* mat = nullptr);

  void SetMaterial(Material* material);

  // Makes the surface a lamp. The material still decides what it reflects;
  // the emission is added on top, from the front face only.
  void SetEmission(std::shared_ptr<Texture> radiance);

  Material* material;
  Primitive* primitive;
  std::shared_ptr<Texture> emission;  // null unless the node is a lamp
  // Aimed at like a lamp, though it may emit nothing. Lamps are aimed at
  // whatever this says.
  bool sampled = false;

  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;
  AABB Bounds() const override;
};

#endif  // RAYTRACER_SRC_SCENE_GEOMETRY_NODE_H_
