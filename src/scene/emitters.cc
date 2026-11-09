#include "scene/emitters.h"

#include <glm/glm.hpp>

#include "scene/geometry_node.h"
#include "scene/material.h"

namespace {

void Collect(const SceneNode& node, const glm::mat4& parent_to_world,
             std::vector<Quad>* emitters) {
  const glm::mat4 to_world = parent_to_world * node.GetTransform();

  if (node.node_type == NodeType::kGeometryNode) {
    const auto& geometry = static_cast<const GeometryNode&>(node);
    const auto* quad = dynamic_cast<const Quad*>(geometry.primitive);
    if (quad != nullptr &&
        dynamic_cast<const DiffuseLight*>(geometry.material) != nullptr) {
      emitters->push_back(quad->Transformed(to_world));
    }
  }

  for (const SceneNode* child : node.children) {
    Collect(*child, to_world, emitters);
  }
}

}  // namespace

std::vector<Quad> CollectEmitters(const SceneNode& root) {
  std::vector<Quad> emitters;
  Collect(root, glm::mat4(1.0f), &emitters);
  return emitters;
}
