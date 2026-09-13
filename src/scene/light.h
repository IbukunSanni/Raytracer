// Termm--Fall 2020

#ifndef RAYTRACER_SRC_SCENE_LIGHT_H_
#define RAYTRACER_SRC_SCENE_LIGHT_H_

#include <glm/glm.hpp>
#include <iosfwd>

// Represents a simple point light.
struct Light {
  Light();

  glm::vec3 colour;
  glm::vec3 position;
  double falloff[3];
};

std::ostream& operator<<(std::ostream& out, const Light& l);

#endif  // RAYTRACER_SRC_SCENE_LIGHT_H_
