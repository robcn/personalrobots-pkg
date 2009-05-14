/*********************************************************************
 * Software License Agreement (BSD License)
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
 *   * Neither the name of Willow Garage nor the names of its
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

/* Author: Wim Meeussen */

#include "door_functions/door_functions.h"
#include "doors_core/action_open_door.h"


using namespace tf;
using namespace KDL;
using namespace ros;
using namespace std;
using namespace door_handle_detector;
using namespace door_functions;

static const string fixed_frame = "odom_combined";


OpenDoorAction::OpenDoorAction(Node& node) :
  robot_actions::Action<door_msgs::Door, door_msgs::Door>("open_door"),
  node_(node)
{
  node_.advertise<manipulation_msgs::TaskFrameFormalism>("r_arm_cartesian_tff_controller/command", 10);
};



OpenDoorAction::~OpenDoorAction()
{
  node_.unadvertise("r_arm_cartesian_tff_controller/command");
};



robot_actions::ResultStatus OpenDoorAction::execute(const door_msgs::Door& goal, door_msgs::Door& feedback)
{ 
  ROS_INFO("OpenDoorAction: execute");



  // open door
  tff_door_.mode.vel.x = tff_door_.VELOCITY;
  tff_door_.mode.vel.y = tff_door_.FORCE;
  tff_door_.mode.vel.z = tff_door_.FORCE;
  tff_door_.mode.rot.x = tff_door_.FORCE;
  tff_door_.mode.rot.y = tff_door_.FORCE;
  tff_door_.mode.rot.z = tff_door_.POSITION;
  
  tff_door_.value.vel.x = getDoorDir(goal) * 0.45;
  tff_door_.value.vel.y = 0.0;
  tff_door_.value.vel.z = 0.0;
  tff_door_.value.rot.x = 0.0;
  tff_door_.value.rot.y = 0.0;
  tff_door_.value.rot.z = 0.0;
  
  // open door
  while (!isPreemptRequested()){
    node_.publish("r_arm_cartesian_tff_controller/command", tff_door_);
    Duration(0.1).sleep();
  }

  // preempted
  ROS_INFO("ActionOpenDoor: Preempted");
  return robot_actions::PREEMPTED;
}

