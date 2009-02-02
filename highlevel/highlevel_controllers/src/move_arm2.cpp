/********************************************************************* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* This is a new version of move_arm, which will eventually replace
 * move_arm. - BPG */

/**
 * @mainpage
 *
 * @htmlinclude manifest.html
 *
 * @b move_arm is a highlevel controller for moving an arm to a goal joint configuration.
 *
 * This node uses the kinematic path planning service provided in the ROS ompl library where a
 * range of motion planners are available. The control is accomplished by incremental dispatch
 * of joint positions reflecting waypoints in the computed path, until all are accomplished. The current
 * implementation is limited to operatng on left or right arms of a pr2 and is dependent on
 * the kinematic model of that robot.
 *
 * <hr>
 *
 *  @section usage Usage
 *  @verbatim
 *  $ move_arm left|right
 *  @endverbatim
 *
 * <hr>
 *
 * @section topic ROS topics
 *
 * Subscribes to (name/type):
 * - @b "mechanism_state"/robot_msgs::MechanismState : The state of the robot joints and actuators
 * - @b "right_arm_goal"/pr2_msgs::MoveArmGoal : The new goal containing a setpoint to achieve for the joint angles
 * - @b "left_arm_goal"/pr2_msgs::MoveArmGoal : The new goal containing a setpoint to achieve for the joint angles
 *
 * Publishes to (name / type):
 * - @b "left_arm_state"/pr2_msgs::MoveArmState : The published state of the controller
 * - @b "left_arm_commands"/pr2_controllers::JointPosCmd : A commanded joint position for the right arm
 *
 *  <hr>
 *
 * @section parameters ROS parameters
 *
 * - None
 **/

#include <highlevel_controllers/highlevel_controller.hh>

// Our interface to the executive
#include <pr2_msgs/MoveArmState.h>
#include <pr2_msgs/MoveArmGoal.h>

#include <robot_srvs/KinematicReplanState.h>
#include <robot_srvs/KinematicReplanLinkPosition.h>
#include <robot_msgs/DisplayKinematicPath.h>
#include <robot_msgs/KinematicPlanStatus.h>

#include <std_msgs/Empty.h>
#include <robot_msgs/JointTraj.h>
#include <pr2_mechanism_controllers/TrajectoryStart.h>
#include <pr2_mechanism_controllers/TrajectoryQuery.h>
#include <pr2_mechanism_controllers/TrajectoryCancel.h>

#include <planning_models/kinematic.h>

#include <sstream>
#include <cassert>

#include <tf/transform_listener.h>

class MoveArm : public HighlevelController<pr2_msgs::MoveArmState, 
                                           pr2_msgs::MoveArmGoal>
{

public:

  /**
   * @brief Constructor
   */
  MoveArm(const std::string& node_name, 
          const std::string& state_topic, 
          const std::string& goal_topic,
          const std::string& kinematic_model,
	  const std::string& controller_name);

  virtual ~MoveArm();

private:
  const std::string kinematic_model_;
  const std::string controller_name_;
  planning_models::KinematicModel* robot_model_;

  bool                      have_new_traj_;
  robot_msgs::KinematicPath new_traj_;

  bool replanning_;
  int  traj_id_;
  int  plan_id_;

  robot_srvs::KinematicReplanState::Request        active_request_state_;
  robot_srvs::KinematicReplanLinkPosition::Request active_request_link_position_;
  robot_msgs::KinematicPlanStatus                  kps_msg_;

  // HighlevelController interface that we must implement
  virtual void updateGoalMsg();
  virtual bool makePlan();
  virtual bool goalReached();
  virtual bool dispatchCommands();

  // Internal helpers
  void requestStopReplanning(void);
  void sendArmCommand(robot_msgs::KinematicPath &path, 
                      const std::string &model);
  void stopArm(void);
  void getTrajectoryMsg(robot_msgs::KinematicPath &path, 
                        robot_msgs::JointTraj &traj);
  void printKinematicPath(robot_msgs::KinematicPath &path);
  void kpsCallback();
};

MoveArm::~MoveArm() {
  if (robot_model_) {
    delete robot_model_;
  }
}

MoveArm::MoveArm(const std::string& node_name, 
                 const std::string& state_topic, 
                 const std::string& goal_topic,
                 const std::string& kinematic_model,
		 const std::string& controller_name)
  : HighlevelController<pr2_msgs::MoveArmState, pr2_msgs::MoveArmGoal>(node_name, state_topic, goal_topic),
    kinematic_model_(kinematic_model),
    controller_name_(controller_name),
    robot_model_(NULL),
    have_new_traj_(false),
    replanning_(false),
    traj_id_(-1),
    plan_id_(-1)
{
  ros::Node::subscribe("kinematic_planning_status",
                       kps_msg_,
                       &MoveArm::kpsCallback,
                       1);
  
  advertise<std_msgs::Empty>("replan_stop", 1);
  advertise<std_msgs::Empty>("replan_force", 1);

  //Create robot model.
  std::string model;
  if (getParam("robot_description", model))
  {
    robot_desc::URDF file;
    file.loadString(model.c_str());
    robot_model_ = new planning_models::KinematicModel();
    robot_model_->setVerbose(false);
    robot_model_->build(file);
    // make sure we are in the robot's self frame
    robot_model_->reduceToRobotFrame();
    
    // Say that we're up and ready
    initialize();
  }
  else
    ROS_ERROR("Robot model not found! Did you remap robot_description?");
}

void MoveArm::updateGoalMsg()
{
  lock();
  stateMsg.configuration = goalMsg.configuration;
  unlock();
}

bool MoveArm::makePlan()
{
  // Not clear whether we need to call lock() here...

  /*if(!goalMsg.implicit_goal)
    {*/
  robot_srvs::KinematicReplanState::Request  req;
  robot_srvs::KinematicReplanState::Response  res;
  
  req.value.params.model_id = kinematic_model_;
  req.value.params.distance_metric = "L2Square";
  req.value.params.planner_id = "SBL";
  req.value.threshold = 0.01;
  req.value.interpolate = 1;
  req.value.times = 1;
  
  // req.start_state is left empty, because we only support replanning, in
  // which case the planner monitors the robot's current state.
  

  //Copies in the state.
  //First create stateparams for the group of intrest.
  planning_models::KinematicModel::StateParams *state = robot_model_->newStateParams();
  
  //Set the stateparam's values from the goal (need to be locked).
  goalMsg.lock();
  for (unsigned int i = 0; i < goalMsg.configuration.size(); i++) 
  {
    unsigned int axes = robot_model_->getJoint(goalMsg.configuration[i].name)->usedParams;
    ROS_ASSERT(axes == 1);
    double* param = new double[axes];
    param[0] = goalMsg.configuration[i].position;
    state->setParams(param, goalMsg.configuration[i].name);
    delete[] param;
  }
  goalMsg.unlock();
    
  //Debug
  //state.print();

  //Copy the stateparams in to the req.
  unsigned int len = robot_model_->getGroupDimension(robot_model_->getGroupID(kinematic_model_));
  double* param = new double[len];
  state->copyParams(param, robot_model_->getGroupID(kinematic_model_));
  
  delete state;
  
  
  req.value.goal_state.vals.clear();
  for (unsigned int i = 0; i < len; i++) {
    //ROS_INFO("%f", param[i]);
    req.value.goal_state.vals.push_back(param[i]);
  }

  delete[] param;
  
  req.value.allowed_time = 0.5;
  
  //req.params.volume* are left empty, because we're not planning for the
  //base
  //Lock here to prevent issues where plan_id_ is not set and a plan is gotten.
  kps_msg_.lock();

  bool ret = false;
  if(replanning_)
    requestStopReplanning();
  replanning_ = true;
  active_request_state_ = req;
  if(!ros::service::call("replan_kinematic_path_state", req, res))
  {
    ROS_WARN("Service call on replan_kinematic_path_state failed");
    ret = false;
  }
  else
  {
    plan_id_ = res.id;
    ROS_INFO("Issued a replanning request. Waiting for plan ID %d", plan_id_);	    
    ret = true;
  }
  kps_msg_.unlock();

  return ret;
    /*}
  else
  {
    robot_srvs::KinematicReplanLinkPosition::Request req;
    robot_srvs::KinematicReplanLinkPosition::Response res;
    req.value.params.model_id = kinematic_model_;
    req.value.params.distance_metric = "L2Square";
    req.value.params.planner_id = "IKSBL";
    req.value.interpolate = 1;
    req.value.times = 1;

    // req.start_state is left empty, because we only support replanning, in
    // which case the planner monitors the robot's current state.

    req.value.goal_constraints = goalMsg.goal_constraints;

    req.value.allowed_time = 0.3;

    //req.params.volume* are left empty, because we're not planning for the
    //base

    if(replanning_)
      requestStopReplanning();
    replanning_ = true;
    active_request_link_position_ = req;
    if(!ros::service::call("replan_kinematic_path_position", req, res))
    {
      ROS_WARN("Service call on replan_kinematic_path_position failed");
      return false;
    }
    else
    {
      plan_id_ = res.id;
      ROS_INFO("Issued a replanning request");	    
      return true;
    }
    }*/
}

bool MoveArm::goalReached()
{
  kps_msg_.lock();
  bool ret = kps_msg_.done;
  kps_msg_.unlock();
  return ret;
}

bool MoveArm::dispatchCommands()
{
  // To prevent a new one from being written in while we're looking at it
  kps_msg_.lock();
  if(have_new_traj_)
  {
    sendArmCommand(new_traj_, kinematic_model_);
    have_new_traj_ = false;
  }
  kps_msg_.unlock();
  return true;
}

void MoveArm::requestStopReplanning(void)
{
  std_msgs::Empty dummy;
  ros::Node::publish("replan_stop", dummy);
  replanning_ = false;
  ROS_INFO("Issued a request to stop replanning");	
}

void MoveArm::printKinematicPath(robot_msgs::KinematicPath &path)
{
    unsigned int nstates = path.get_states_size();    
    ROS_INFO("Obtained solution path with %u states", nstates);    
    std::stringstream ss;
    ss << std::endl;
    for (unsigned int i = 0 ; i < nstates ; ++i)
    {
	for (unsigned int j = 0 ; j < path.states[i].get_vals_size() ; ++j)
	    ss <<  path.states[i].vals[j] << " ";	
	ss << std::endl;	
    }    
    ROS_INFO(ss.str().c_str());
}

void MoveArm::getTrajectoryMsg(robot_msgs::KinematicPath &path, 
                               robot_msgs::JointTraj &traj)
{
  traj.set_points_size(path.get_states_size());

  for (unsigned int i = 0 ; i < path.get_states_size() ; ++i)
  {
    traj.points[i].set_positions_size(path.states[i].get_vals_size());
    for (unsigned int j = 0 ; j < path.states[i].get_vals_size() ; ++j)
      traj.points[i].positions[j] = path.states[i].vals[j];
    traj.points[i].time = 0.0;
  }	
}

void MoveArm::sendArmCommand(robot_msgs::KinematicPath &path, 
                             const std::string &model)
{
  pr2_mechanism_controllers::TrajectoryStart::Request  send_traj_start_req;
  pr2_mechanism_controllers::TrajectoryStart::Response send_traj_start_res;
  robot_msgs::JointTraj traj;
  getTrajectoryMsg(path, traj);
  send_traj_start_req.traj = traj;
  if(!ros::service::call(controller_name_ + "/TrajectoryStart", send_traj_start_req, send_traj_start_res))
    ROS_WARN("Failed to send trajectory to controller");
  else
  {
    traj_id_ = send_traj_start_res.trajectoryid;
    printKinematicPath(path);
    ROS_INFO("Sent trajectory to controller");
  }
}

void MoveArm::stopArm()
{
  if(traj_id_ < 0)
    ROS_INFO("No trajectory to stop");
  else
  {
    pr2_mechanism_controllers::TrajectoryCancel::Request  stop_traj_start_req;
    pr2_mechanism_controllers::TrajectoryCancel::Response stop_traj_start_res;
    stop_traj_start_req.trajectoryid = 0;  // make sure we stop all trajectories    
    if(!ros::service::call(controller_name_ + "/TrajectoryCancel",
                           stop_traj_start_req, stop_traj_start_res))
      ROS_WARN("Failed to cancel trajectory");
    else
    {
      ROS_INFO("Cancelled trajectory");
      traj_id_ = -1;
    }
  }
}

void MoveArm::kpsCallback()
{
  if(!kps_msg_.valid || kps_msg_.unsafe)
    stopArm();
  else
  {
    if(kps_msg_.id >= 0 && (kps_msg_.id == plan_id_))
    {
      if(!kps_msg_.path.states.empty())
      {
	have_new_traj_ = true;
	new_traj_ = kps_msg_.path;
      }
      
      // by the time have_new_traj_ is looked at, it could be the case a
      // new message is received and the trajectory is lost
    } 
  }
}

class MoveRightArm: public MoveArm 
{
public:
  MoveRightArm(): MoveArm("rightArmController", 
                          "right_arm_state", 
                          "right_arm_goal", 
                          "pr2::right_arm",
			  "right_arm_trajectory_controller")
  {
  };

protected:
};

class MoveLeftArm: public MoveArm 
{
public:
  MoveLeftArm(): MoveArm("leftArmController", 
                         "left_arm_state", 
                         "left_arm_goal", 
                         "pr2::left_arm",
			 "left_arm_trajectory_controller")
  {
  }
};

void usage(void)
{
    std::cout << "Usage: ./move_arm left|right [standard ROS arguments]" << std::endl;
}

int
main(int argc, char** argv)
{

  if(argc < 2){
    usage();
    return -1;
  }

  ros::init(argc,argv);

  // Extract parameters
  const std::string param = argv[1];

  if(param == "left"){
    MoveLeftArm node;
    node.run();
    ros::fini();
  }
  else if(param == "right"){
    MoveRightArm node;
    node.run();
    ros::fini();
  }
  else {
    usage();      
    return -1;
  }

  return(0);
}
