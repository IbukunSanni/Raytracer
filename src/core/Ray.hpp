#pragma once


#include <glm/glm.hpp>
using namespace std;
using namespace glm;

class Ray {
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

    Ray(){
        originVec = vec3();
        dirVec = vec3();
    }

};



