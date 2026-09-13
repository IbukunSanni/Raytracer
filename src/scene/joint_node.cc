// Termm--Fall 2020

#include "scene/joint_node.h"

//---------------------------------------------------------------------
JointNode::JointNode(const std::string& name) : SceneNode(name) {
  node_type = NodeType::kJointNode;
}

//---------------------------------------------------------------------
JointNode::~JointNode() {}
//---------------------------------------------------------------------
void JointNode::SetJointX(double min, double init, double max) {
  joint_x.min = min;
  joint_x.init = init;
  joint_x.max = max;
}

//---------------------------------------------------------------------
void JointNode::SetJointY(double min, double init, double max) {
  joint_y.min = min;
  joint_y.init = init;
  joint_y.max = max;
}
