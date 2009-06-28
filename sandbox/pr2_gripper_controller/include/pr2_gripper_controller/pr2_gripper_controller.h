/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
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
 /*
 * Author: Sachin Chitta and Matthew Piccoli
 */ 
 
#include <ros/node.h>
#include <mechanism_model/robot.h>
#include <mechanism_model/controller.h>
#include <robot_mechanism_controllers/joint_effort_controller.h>
#include <pr2_gripper_controller/GripperControllerCmd.h>

namespace controller
{
  class Pr2GripperController : public Controller
  {
    public:
    /*!
    * \brief Loads controller's information from the xml description file and param server
    * @param robot_state The robot's current state
    * @param config Tiny xml element pointing to this controller
    * @return Successful init
    */
    bool initXml(mechanism::RobotState *robot, TiXmlElement *config);	
    
    /*!
	* \brief (a) Updates commands to the gripper.
	* Called every timestep in realtime
	*/
	void update();	
	
	bool starting();
    
    double rampMove(double start_force, double end_force, double time);
    
    double stepMove(double step_size);
    
    double grasp();
    
    private:
    std::string name_;
    
    double default_speed_;
    
    void command_callback();
    
    pr2_gripper_controller::GripperControllerCmd grasp_cmd_desired_;
    
    pr2_gripper_controller::GripperControllerCmd grasp_cmd_;
    
    pr2_gripper_controller::GripperControllerCmd grasp_cmd;
    
    /*!
    * \brief mutex lock for setting and getting commands
    */
    pthread_mutex_t pr2_gripper_controller_lock_;
    
    /*!
    * \brief true when new command received by node
    */
    bool new_cmd_available_;
    
    /*!
    * \brief timeout specifying time that the controller waits before setting the current velocity command to zero
    */
    double timeout_;
    
    /*!
    * \brief time corresponding to when update was last called
    */
    double last_time_;
    
    double cmd_received_timestamp_;
    
    /*!
    * \brief remembers everything about the state of the robot
    */
    mechanism::RobotState *robot_state_;
    
    /*!
    * \brief JointState for this caster joint
    */
    mechanism::JointState *joint_;
    
    JointEffortController joint_controller_;
    
    double parseMessage(pr2_gripper_controller::GripperControllerCmd desired_msg);
  };
}



