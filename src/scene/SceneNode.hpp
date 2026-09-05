// Termm--Fall 2020

#pragma once

#include "scene/Material.hpp"

#include <glm/glm.hpp>

#include <list>
#include <string>
#include <iostream>

#include "core/Ray.hpp"
#include "core/HitRecord.hpp"

enum class NodeType {
	SceneNode,
	GeometryNode,
	JointNode
};

class SceneNode {
public:
    SceneNode(const std::string & name);

	SceneNode(const SceneNode & other);

    virtual ~SceneNode();
    
	int totalSceneNodes() const;
    
    const glm::mat4& get_transform() const;
    const glm::mat4& get_inverse() const;
    
    void set_transform(const glm::mat4& m);
    
    void add_child(SceneNode* child);
    
    void remove_child(SceneNode* child);

	//-- Transformations:
    void rotate(char axis, float angle);
    void scale(const glm::vec3& amount);
    void translate(const glm::vec3& amount);


	friend std::ostream & operator << (std::ostream & os, const SceneNode & node);

    // Transformations
    glm::mat4 trans;
    glm::mat4 invtrans;
    
    std::list<SceneNode*> children;

	NodeType m_nodeType;
	std::string m_name;
	unsigned int m_nodeId;

    virtual bool isHit(Ray & ray,float t0Float,float t1Float, HitRecord &record );

protected:
    // Ray transport helpers. The transform must be applied exactly ONCE per
    // node: toLocal on the way in, toWorld on the way out. GeometryNode used
    // to transform and then delegate to SceneNode::isHit, which transformed
    // again with the same matrix -- so anything parented to a GeometryNode
    // was displaced. These exist so both node types share one code path.
    Ray toLocal(Ray & ray) const;
    void toWorld(HitRecord & record) const;

    // Intersect this node's children with a ray ALREADY in local space.
    // Does not transform: the caller has done it, and each child applies
    // its own transform inside its own isHit.
    bool hitChildren(Ray & localRay,float t0Float,float t1Float, HitRecord &record );

private:
	// The number of SceneNode instances.
	static unsigned int nodeInstanceCount;
};