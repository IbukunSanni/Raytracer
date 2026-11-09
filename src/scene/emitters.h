// Raytracer -- the area lights in a scene, found by walking its graph

#ifndef RAYTRACER_SRC_SCENE_EMITTERS_H_
#define RAYTRACER_SRC_SCENE_EMITTERS_H_

#include <vector>

#include "geometry/primitive.h"
#include "scene/scene_node.h"

// Every quad at or below `root` whose node emits, in world space:
// carried through its own node's transform and all its ancestors', `root`'s
// included. A path's hit points are in world space, so a quad left in its
// node's space would be aimed at in the wrong place and have the wrong area.
//
// Only quads: a sphere that emits still shines on a path that happens
// to hit it, but it is not listed here, so it cannot be aimed at.
std::vector<Quad> CollectEmitters(const SceneNode& root);

#endif  // RAYTRACER_SRC_SCENE_EMITTERS_H_
