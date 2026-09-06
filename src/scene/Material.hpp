#pragma once

#include <glm/glm.hpp>

class Rng;

class Material
{
public:
  virtual ~Material();

  virtual glm::vec3 eval(const glm::vec3 &in,
                         const glm::vec3 &normal,
                         const glm::vec3 &out) const = 0;

  // Solid-angle density that sample() would have drawn `out` with.
  virtual float pdf(const glm::vec3 & in,
                    const glm::vec3 & normal,
                    const glm::vec3 & out) const = 0;
  
  // Draw the next direction; writes the pdf and BRDF for the direction
  // chosen, so a caller needing both does not pay for a second dispatch.
  virtual glm::vec3 sample(Rng & rng,
                           const glm::vec3 & in,
                           const glm::vec3 & normal,
                           float * pdf,
                           glm::vec3 * brdf) const = 0;
  
  // legacy implementation: Virtual here purely so Renderer.cpp can drop its static_cast.
  virtual glm::vec3 getDiffuse()   const = 0;
  virtual glm::vec3 getSpecular()  const = 0;
  virtual double    getShininess() const = 0;

protected:
  Material();
};
