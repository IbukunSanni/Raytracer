// TODO: check hit record definition

#pragma once

#include "Material.hpp"

#include <glm/glm.hpp>
using namespace glm;

// TODO: use getters and setters
class HitRecord{
    public:
        float t;
        vec3 hitPointVec;
        vec3 normalVec;
        Material *material;

};