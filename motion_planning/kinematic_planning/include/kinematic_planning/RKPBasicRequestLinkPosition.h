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

/** \author Ioan Sucan */

#ifndef KINEMATIC_PLANNING_RPK_BASIC_REQUEST_LINK_POSITION_
#define KINEMATIC_PLANNING_RPK_BASIC_REQUEST_LINK_POSITION_

#include "kinematic_planning/RKPBasicRequest.h"
#include <robot_srvs/KinematicPlanLinkPosition.h>
#include <robot_srvs/KinematicReplanLinkPosition.h>

namespace kinematic_planning
{

    static const int R_POSITION = 2; // link position request
    
    class GoalToPosition : public ompl::SpaceInformationKinematic::GoalRegionKinematic
    {
    public:
	
        GoalToPosition(ompl::SpaceInformation_t si, RKPModel *model, const std::vector<robot_msgs::PoseConstraint> &pc) : ompl::SpaceInformationKinematic::GoalRegionKinematic(si)
	{
	    m_model = model;
	    for (unsigned int i = 0 ; i < pc.size() ; ++i)
	    {
		PoseConstraintEvaluator *pce = new PoseConstraintEvaluator();
		pce->use(m_model->kmodel, pc[i]);
		m_pce.push_back(pce);
		
		// if we have position constraints
		if (pc[i].type & 0xFF)
		    threshold += pc[i].position_distance;
		
		// if we have orientation constraints
		if (pc[i].type & (~0xFF))
		    threshold += ((pc[i].type & 0xFF) ? pc[i].orientation_importance : 1.0) * pc[i].orientation_distance;
	    }
	}
	
	virtual ~GoalToPosition(void)
	{
	    for (unsigned int i = 0 ; i < m_pce.size() ; ++i)
		delete m_pce[i];
	}
	
	virtual double distanceGoal(ompl::SpaceInformationKinematic::StateKinematic_t state) const
	{
	    return evaluateGoalAux(state, NULL);
	}
	
	virtual bool isSatisfied(ompl::SpaceInformation::State_t state, double *dist) const
	{
	    std::vector<bool> decision;
	    double d = evaluateGoalAux(static_cast<ompl::SpaceInformationKinematic::StateKinematic_t>(state), &decision);
	    if (dist)
		*dist = d;
	    
	    for (unsigned int i = 0 ; i < decision.size() ; ++i)
		if (!decision[i])
		    return false;
	    return true;
	}
	
	virtual void print(std::ostream &out = std::cout) const
	{
	    ompl::SpaceInformationKinematic::GoalRegionKinematic::print(out);
	    for (unsigned int i = 0 ; i < m_pce.size() ; ++i)
		m_pce[i]->print(out);
	}
	
    protected:
	
	double evaluateGoalAux(ompl::SpaceInformationKinematic::StateKinematic_t state, std::vector<bool> *decision) const
	{
	    update(state);
	    
	    if (decision)
		decision->resize(m_pce.size());
	    double distance = 0.0;
	    for (unsigned int i = 0 ; i < m_pce.size() ; ++i)
	    {
		double dPos, dAng;
		m_pce[i]->evaluate(&dPos, &dAng);
		if (decision)
		    (*decision)[i] = m_pce[i]->decide(dPos, dAng);
		distance += dPos + m_pce[i]->getConstraintMessage().orientation_importance * dAng;
	    }
	    
	    return distance;
	}

	// this function shuld be thread safe, but it is not
	void update(ompl::SpaceInformationKinematic::StateKinematic_t state) const
	{
	    m_model->kmodel->computeTransforms(static_cast<const ompl::SpaceInformationKinematic::StateKinematic_t>(state)->values, m_model->groupID);
	    m_model->collisionSpace->updateRobotModel(m_model->collisionSpaceID);
	}    
	
	mutable RKPModel                      *m_model;
	std::vector<PoseConstraintEvaluator*>  m_pce;
	
    };

    template<>
    RKPBasicRequest<robot_msgs::KinematicPlanLinkPositionRequest>::RKPBasicRequest(void)
    {
	type = R_POSITION;
    }

    /** Validate request for planning towards a link position */
    template<>
    bool RKPBasicRequest<robot_msgs::KinematicPlanLinkPositionRequest>::isRequestValid(ModelMap &models, robot_msgs::KinematicPlanLinkPositionRequest &req)
    {
	if (!areSpaceParamsValid(models, req.params))
	    return false;
	
	RKPModel *m = models[req.params.model_id];
	
	if (m->kmodel->getModelInfo().stateDimension != req.start_state.get_vals_size())
	{
	    ROS_ERROR("Dimension of start state expected to be %d but was received as %d", m->kmodel->getModelInfo().stateDimension, req.start_state.get_vals_size());
	    return false;
	}
	
	return true;
    }
    
    /** Set the goal using a destination link position */
    template<>
    void RKPBasicRequest<robot_msgs::KinematicPlanLinkPositionRequest>::setupGoalState(RKPModel *model, RKPPlannerSetup *psetup, robot_msgs::KinematicPlanLinkPositionRequest &req)
    {
	/* set the goal */
	std::vector<robot_msgs::PoseConstraint> pc;
	req.get_goal_constraints_vec(pc);
	
	GoalToPosition *goal = new GoalToPosition(psetup->si, model, pc);
	psetup->si->setGoal(goal); 
    }

} // kinematic_planning

#endif
