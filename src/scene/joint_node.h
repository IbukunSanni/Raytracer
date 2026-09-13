// Termm--Fall 2020

#ifndef RAYTRACER_SRC_SCENE_JOINT_NODE_H_
#define RAYTRACER_SRC_SCENE_JOINT_NODE_H_

#include "scene/scene_node.h"

class JointNode : public SceneNode {
 public:
  explicit JointNode(const std::string& name);
  ~JointNode() override;

  void SetJointX(double min, double init, double max);
  void SetJointY(double min, double init, double max);

  struct JointRange {
    double min, init, max;
  };

  JointRange joint_x, joint_y;
};

#endif  // RAYTRACER_SRC_SCENE_JOINT_NODE_H_
