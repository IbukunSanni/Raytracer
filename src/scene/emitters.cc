#include "scene/emitters.h"

#include <glm/glm.hpp>
#include <optional>

#include "scene/geometry_node.h"

namespace {

void Collect(const SceneNode& node, const glm::mat4& parent_to_world,
             SampledShapes* shapes, size_t* skipped) {
  const glm::mat4 to_world = parent_to_world * node.GetTransform();

  if (node.node_type == NodeType::kGeometryNode) {
    const auto& geometry = static_cast<const GeometryNode&>(node);
    if (geometry.emission != nullptr || geometry.sampled) {
      const Primitive* primitive = geometry.primitive;
      if (const auto* quad = dynamic_cast<const Quad*>(primitive)) {
        shapes->quads.push_back(quad->Transformed(to_world));
      } else if (const auto* ball =
                     dynamic_cast<const NonhierSphere*>(primitive)) {
        std::optional<NonhierSphere> world = ball->Transformed(to_world);
        if (world && !ball->IsMoving()) {
          shapes->spheres.push_back(*world);
        } else {
          ++*skipped;
        }
      } else if (dynamic_cast<const Sphere*>(primitive) != nullptr) {
        // The unit sphere at the origin, sized and placed by its node.
        std::optional<NonhierSphere> world =
            NonhierSphere(glm::vec3(0.0f), 1.0).Transformed(to_world);
        if (world) {
          shapes->spheres.push_back(*world);
        } else {
          ++*skipped;
        }
      }
    }
  }

  for (const SceneNode* child : node.children) {
    Collect(*child, to_world, shapes, skipped);
  }
}

}  // namespace

SampledShapes CollectSampledShapes(const SceneNode& root, size_t* skipped) {
  SampledShapes shapes;
  size_t ignored = 0;
  Collect(root, glm::mat4(1.0f), &shapes, skipped ? skipped : &ignored);
  return shapes;
}
