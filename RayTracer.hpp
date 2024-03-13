#pragma once


#include <glm/glm.hpp>
using namespace std;
using namespace glm;

class RayTracer {
private:
    vec3 originVec;
    vec3 dirVec;
public:
    void setOrigin(const vec3& oVec){
        originVec = oVec;
    }

    vec3 getOrigin(){
        return originVec;
    }

    void setDirection(const vec3& dVec){
        dirVec = dVec;
    }

    vec3 getDirection(){
        return dirVec;
    }

    vec3 getPointAtT(float tFloat){
        return originVec + tFloat * dirVec;
    }

    RayTracer(){
        originVec = vec3();
        dirVec = vec3();
    }

};



