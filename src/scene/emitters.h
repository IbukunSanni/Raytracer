// Raytracer -- the shapes a path aims at, found by walking the scene graph

#ifndef RAYTRACER_SRC_SCENE_EMITTERS_H_
#define RAYTRACER_SRC_SCENE_EMITTERS_H_

#include <cstddef>
#include <vector>

#include "geometry/primitive.h"
#include "scene/scene_node.h"

// What the mixture aims half its bounces at, in world space. Every lamp, and
// every shape whose node asked to be aimed at without glowing: glass that
// focuses a lamp's light is worth aiming at too. Aiming at a shape that
// emits nothing changes where the noise falls, never the mean, because the
// density divided by is always the one the direction was drawn from.
struct SampledShapes {
  std::vector<Quad> quads;
  std::vector<NonhierSphere> spheres;

  size_t Count() const { return quads.size() + spheres.size(); }
  bool IsEmpty() const { return Count() == 0; }
};

// Every quad and sphere at or below `root` whose node emits or is marked to
// be sampled, carried through its own node's transform and all its
// ancestors', `root`'s included. A path's hit points are in world space, so a
// shape left in its node's space would be aimed at in the wrong place.
//
// Left out, and still lit by any path that happens to hit them: other
// primitives, a moving sphere, and a sphere under a transform that stretches
// it into an ellipsoid. `skipped`, if given, counts the last two.
SampledShapes CollectSampledShapes(const SceneNode& root,
                                   size_t* skipped = nullptr);

#endif  // RAYTRACER_SRC_SCENE_EMITTERS_H_
