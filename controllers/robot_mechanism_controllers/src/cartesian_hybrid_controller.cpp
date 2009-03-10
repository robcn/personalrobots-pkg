/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Author: Stuart Glaser

#ifndef CARTESIAN_HYBRID_CONTROLLER_H
#define CARTESIAN_HYBRID_CONTROLLER_H

#include "robot_mechanism_controllers/cartesian_hybrid_controller.h"
#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/chainfksolvervel_recursive.hpp>
#include <kdl/chainjnttojacsolver.hpp>
#include <kdl/jacobian.hpp>
#include "realtime_tools/realtime_publisher.h"
#include "control_toolbox/pid.h"
#include "robot_msgs/VisualizationMarker.h"

namespace controller {

CartesianHybridController::CartesianHybridController()
  : robot_(NULL), last_time_(0)
{
}

bool CartesianHybridController::initXml(mechanism::RobotState *robot, TiXmlElement *config)
{
  assert(robot);
  robot_ = robot;
  std::string name = config->Attribute("name") ? config->Attribute("name") : "";
  if (name.empty())
  {
    ROS_ERROR("No name for CartesianHybridController");
    return false;
  }

  ros::Node *node = ros::Node::instance();

  // Chain

  std::string root_link, tip_link;
  if (!node->getParam(name + "/root_link", root_link)) {
    ROS_ERROR("No root link specified");
    return false;
  }
  if (!node->getParam(name + "/tip_link", tip_link)) {
    ROS_ERROR("No tip link specified");
    return false;
  }
  if (!chain_.init(robot->model_, root_link, tip_link))
    return false;
  chain_.toKDL(kdl_chain_);

  // Pids

  control_toolbox::Pid temp_pid;

  if (!temp_pid.initParam(name + "/fb_pose"))
    return false;
  for (size_t i = 0; i < 6; ++i)
    pose_pids_[i] = temp_pid;

  if (!temp_pid.initParam(name + "/fb_trans_vel"))
    return false;
  for (size_t i = 0; i < 3; ++i)
    twist_pids_[i] = temp_pid;

  if (!temp_pid.initParam(name + "/fb_rot_vel"))
    return false;
  for (size_t i = 3; i < 6; ++i)
    twist_pids_[i] = temp_pid;

  // Default commands

  task_frame_offset_ = KDL::Frame::Identity();
  tool_frame_offset_ = KDL::Frame::Identity();

  return true;
}

void CartesianHybridController::update()
{
  if (!chain_.allCalibrated(robot_->joint_states_))
    return;
  double time = robot_->hw_->current_time_;
  double dt = time - last_time_;
  last_time_ = time;

  // Measures the current pose and twist

  // Finds the pose/twist of the ee frame w.r.t. the chain root
  KDL::JntArrayVel jnt_vel(kdl_chain_.getNrOfJoints());
  chain_.getVelocities(robot_->joint_states_, jnt_vel);
  KDL::FrameVel ee_in_root;
  KDL::ChainFkSolverVel_recursive fkvel_solver(kdl_chain_);
  fkvel_solver.JntToCart(jnt_vel, ee_in_root);

  // The pose/twist of the tool frame w.r.t. the task frame
  KDL::FrameVel tool = task_frame_offset_.Inverse() * ee_in_root * tool_frame_offset_;
  pose_meas_ = tool.GetFrame();
  twist_meas_ = tool.GetTwist();

  // Computes the desired wrench from the command

  // Caches the roll/pitch/yaw of the tool
  double rpy[3];
  tool.M.R.GetRPY(rpy[0], rpy[1], rpy[2]);

  for (int i = 0; i < 6; ++i)
  {
    twist_desi_[i] = 0;
    wrench_desi_[i] = 0;

    double setpoint = setpoint_[i];
    switch(mode_[i])
    {
    case robot_msgs::TaskFrameFormalism::POSITION:
      if (i < 3) // Translational position
        setpoint = pose_pids_[i].updatePid(tool.p.p[i] - setpoint, dt);
      else // Rotational position
        setpoint = pose_pids_[i].updatePid(rpy[i - 3] - setpoint, dt);
    case robot_msgs::TaskFrameFormalism::VELOCITY:
      twist_desi_[i] = setpoint;
      setpoint = twist_pids_[i].updatePid(tool.GetTwist()[i] - setpoint, dt);
    case robot_msgs::TaskFrameFormalism::FORCE:
      wrench_desi_[i] = setpoint;
      break;
    default:
      abort();
    }
  }

  // Transforms the wrench from the task frame to the chain root frame
  KDL::Wrench wrench_in_root;
  wrench_in_root.force = task_frame_offset_.M * wrench_desi_.force;
  wrench_in_root.torque = task_frame_offset_.M * wrench_desi_.torque;

  // Finds the Jacobian for the tool
  KDL::ChainJntToJacSolver jac_solver(kdl_chain_);
  KDL::Jacobian ee_jacobian(kdl_chain_.getNrOfJoints());
  KDL::Jacobian jacobian(kdl_chain_.getNrOfJoints());  // Tool Jacobian
  jac_solver.JntToJac(jnt_vel.q, ee_jacobian);
  KDL::changeRefFrame(ee_jacobian, tool_frame_offset_, jacobian);

  // jnt_eff = jacobian * wrench
  KDL::JntArray jnt_eff(kdl_chain_.getNrOfJoints());
  for (size_t i = 0; i < kdl_chain_.getNrOfJoints(); ++i)
  {
    jnt_eff(i) = 0;
    for (size_t j = 0; j < 6; ++j)
      jnt_eff(i) += jacobian(j,i) * wrench_in_root(j);
  }

  chain_.setEfforts(jnt_eff, robot_->joint_states_);
}

bool CartesianHybridController::starting()
{
  for (size_t i = 0; i < 6; ++i)
  {
    mode_[i] = robot_msgs::TaskFrameFormalism::FORCE;
    setpoint_[i] = 0.0;
  }
  return true;
}

ROS_REGISTER_CONTROLLER(CartesianHybridControllerNode)

CartesianHybridControllerNode::CartesianHybridControllerNode()
: TF(*ros::Node::instance(), false, ros::Duration(10.0))
{
}

CartesianHybridControllerNode::~CartesianHybridControllerNode()
{
  ros::Node *node = ros::Node::instance();
  node->unsubscribe(name_ + "/command");
  node->unadvertiseService(name_ + "/set_tool_frame");
}

bool CartesianHybridControllerNode::initXml(mechanism::RobotState *robot, TiXmlElement *config)
{
  if (!c_.initXml(robot, config))
    return false;

  if (!config->Attribute("name"))
    return false;
  name_ = config->Attribute("name");

  ros::Node *node = ros::Node::instance();

  node->subscribe(name_ + "/command", command_msg_, &CartesianHybridControllerNode::command, this, 1);
  node->advertiseService(name_ + "/set_tool_frame", &CartesianHybridControllerNode::setToolFrame, this);

  return true;

}

void CartesianHybridControllerNode::update()
{
  c_.update();
}

bool CartesianHybridControllerNode::setTaskFrame(
  robot_srvs::SetPoseStamped::Request &req,
  robot_srvs::SetPoseStamped::Response &resp)
{
  robot_msgs::PoseStamped offset_msg;
  try
  {
    TF.transformPose(c_.chain_.getLinkName(0), req.p, offset_msg);
  }
  catch(tf::TransformException& ex)
  {
    ROS_WARN("Transform Exception %s", ex.what());
    return true;
  }

  tf::Transform offset;
  tf::PoseMsgToTF(offset_msg.pose, offset);
  mechanism::TransformTFToKDL(offset, c_.task_frame_offset_);

  return true;
}

bool CartesianHybridControllerNode::setToolFrame(
  robot_srvs::SetPoseStamped::Request &req,
  robot_srvs::SetPoseStamped::Response &resp)
{
  robot_msgs::PoseStamped offset_msg;
  try
  {
    TF.transformPose(c_.chain_.getLinkName(), req.p, offset_msg);
  }
  catch(tf::TransformException& ex)
  {
    ROS_WARN("Transform Exception %s", ex.what());
    return true;
  }

  tf::Transform offset;
  tf::PoseMsgToTF(offset_msg.pose, offset);
  mechanism::TransformTFToKDL(offset, c_.tool_frame_offset_);

  return true;
}

void CartesianHybridControllerNode::command()
{
  tf::Stamped<tf::Transform> task_frame;
  try {
    TF.lookupTransform(c_.chain_.getLinkName(0), command_msg_.header.frame_id, command_msg_.header.stamp,
                       task_frame);
  }
  catch (tf::TransformException &ex)
  {
    ROS_WARN("Transform Exception %s", ex.what());
    return;
  }
  mechanism::TransformTFToKDL(task_frame, c_.task_frame_offset_);

  c_.mode_[0] = (int)command_msg_.mode.vel.x;
  c_.mode_[1] = (int)command_msg_.mode.vel.y;
  c_.mode_[2] = (int)command_msg_.mode.vel.z;
  c_.mode_[3] = (int)command_msg_.mode.rot.x;
  c_.mode_[4] = (int)command_msg_.mode.rot.y;
  c_.mode_[5] = (int)command_msg_.mode.rot.z;
  c_.setpoint_[0] = command_msg_.value.vel.x;
  c_.setpoint_[1] = command_msg_.value.vel.y;
  c_.setpoint_[2] = command_msg_.value.vel.z;
  c_.setpoint_[3] = command_msg_.value.rot.x;
  c_.setpoint_[4] = command_msg_.value.rot.y;
  c_.setpoint_[5] = command_msg_.value.rot.z;
}

}

#endif
