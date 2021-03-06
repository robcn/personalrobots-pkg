/*********************************************************************
 *
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

 *
 * \author Sachin Chitta, Ioan Sucan
 *********************************************************************/

#include "move_arm/move_arm_setup.h"

bool move_arm::MoveArmSetup::configure(void)
{
    nodeHandle_.param<std::string>("~group", group_, std::string());

    if (group_.empty())
    {
	ROS_ERROR("No '~group' parameter specified. Without the name of the group of joints to plan for, action cannot start");
	return false;
    }

    // monitor robot
    collisionModels_ = new planning_environment::CollisionModels("robot_description");
    planningMonitor_ = new planning_environment::PlanningMonitor(collisionModels_, &tf_);

    if (!collisionModels_->loadedModels())
	return false;

    if (!collisionModels_->getKinematicModel()->hasGroup(group_))
    {
	ROS_ERROR("Group '%s' is not known", group_.c_str());
	return false;
    }
    else
	ROS_INFO("Configuring action for '%s'", group_.c_str());

    if (planningMonitor_->getExpectedJointStateUpdateInterval() > 1e-3)
	planningMonitor_->waitForState();
    if (planningMonitor_->getExpectedMapUpdateInterval() > 1e-3)
	planningMonitor_->waitForMap();

    if (!getControlJointNames(groupJointNames_))
	return false;

    return true;
}

bool move_arm::MoveArmSetup::getControlJointNames(std::vector<std::string> &joint_names)
{
    ros::ServiceClient client_query = nodeHandle_.serviceClient<experimental_controllers::TrajectoryQuery>(CONTROL_QUERY_NAME);
    experimental_controllers::TrajectoryQuery::Request  req_query;
    experimental_controllers::TrajectoryQuery::Response res_query;
    req_query.trajectoryid = experimental_controllers::TrajectoryQuery::Request::Query_Joint_Names;

    bool result = client_query.call(req_query, res_query);

    if (!result)
    {
	ROS_INFO("Querying controller for joint names ...");
	ros::WallDuration(5.0).sleep();
	result = client_query.call(req_query, res_query);
	if (result)
	    ROS_INFO("Joint names received");
    }

    if (!result)
    {
	ROS_ERROR("Unable to retrieve controller joint names from control query service");
	return false;
    }

    joint_names = res_query.jointnames;
    
    planning_models::KinematicModel::JointGroup *jg = planningMonitor_->getKinematicModel()->getGroup(group_);
    
    // make sure we have the right joint names
    for(unsigned int i = 0; i < joint_names.size() ; ++i)
    {
	if (planning_models::KinematicModel::Joint *j = planningMonitor_->getKinematicModel()->getJoint(joint_names[i]))
	{
	    if (!jg->hasJoint(j->name))
	    {
		ROS_ERROR("Joint '%s' is not in group '%s'", j->name.c_str(), group_.c_str());
		return false;
	    }
	    ROS_DEBUG("Using joing '%s' with %u parameters", j->name.c_str(), j->usedParams);
	}
	else
	    return false;
    }

    if (jg->jointNames.size() != joint_names.size())
    {
	ROS_ERROR("The group '%s' does not have the same number of joints as the controller can handle", group_.c_str());
	std::stringstream ss1;
	for (unsigned int i = 0 ; i < jg->jointNames.size() ; ++i)
	    ss1 << jg->jointNames[i] << " ";
	ROS_ERROR("Group '%s': %s", group_.c_str(), ss1.str().c_str());
	
	std::stringstream ss2;
	for (unsigned int i = 0 ; i < joint_names.size() ; ++i)
	    ss2 << joint_names[i] << " ";
	ROS_ERROR("Controller joints: %s", ss2.str().c_str());
	
	return false;
    }

    return true;
}

