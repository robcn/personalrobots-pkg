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

// Original version: Melonee Wise <mwise@willowgarage.com>

#include "pr2_mechanism_controllers/head_pan_tilt_controller.h"

using namespace controller;
using namespace std;

ROS_REGISTER_CONTROLLER(HeadPanTiltController);

HeadPanTiltController::HeadPanTiltController():num_joints_(0),last_time_(0)
{
}

HeadPanTiltController::~HeadPanTiltController()
{
  // Assumes the joint controllers are owned by this class.
  for(unsigned int i=0; i < num_joints_;++i)
  {
    delete joint_position_controllers_[i];
  }
}

bool HeadPanTiltController::initXml(mechanism::RobotState * robot, TiXmlElement * config)
{
  robot_ = robot->model_;
  TiXmlElement *elt = config->FirstChildElement("controller");
  while (elt)
  {
    JointPositionController * jpc = new JointPositionController();
    std::cout<<elt->Attribute("type")<<elt->Attribute("name")<<std::endl;
    assert(static_cast<std::string>(elt->Attribute("type")) == std::string("JointPositionController"));
    joint_position_controllers_.push_back(jpc);

    if(!jpc->initXml(robot, elt))
      return false;

    elt = elt->NextSiblingElement("controller");
  }

  num_joints_ = joint_position_controllers_.size();

  fprintf(stderr,"HeadPanTiltController:: num_joints_: %d\n",num_joints_);

  set_pts_.resize(num_joints_);
  last_time_ = robot->hw_->current_time_;

  return true;
}

void HeadPanTiltController::setJointCmd(const std::vector<double> &j_values, const std::vector<std::string> & j_names)
{
  assert(j_values.size() == j_names.size());
  for(uint i = 0; i < j_values.size(); ++i)
  {
    const std::string & name = j_names[i];
    const int id = getJointControllerByName(name);
    assert(id>=0);
    if(id >= 0)
    {
      set_pts_[id] = j_values[i];
    }
  }
}


void HeadPanTiltController::getJointCmd(pr2_mechanism_controllers::JointCmd & cmd) const
{
  const unsigned int n = joint_position_controllers_.size();
  cmd.set_names_size(n);
  for(unsigned int i=0; i<n; ++i)
      cmd.names[i] = joint_position_controllers_[i]->getJointName();

  cmd.set_positions_vec(set_pts_);
}


controller::JointPositionController* HeadPanTiltController::getJointPositionControllerByName(std::string name)
{
  for(int i=0; i< (int) num_joints_; i++)
  {
    if(joint_position_controllers_[i]->getJointName() == name)
    {
      return joint_position_controllers_[i];
    }
  }
    return NULL;
}


int HeadPanTiltController::getJointControllerByName(std::string name)
{
  for(int i=0; i< (int) num_joints_; i++)
  {
    if(joint_position_controllers_[i]->getJointName() == name)
    {
      return i;
    }
  }
  return -1;
}

void HeadPanTiltController::update(void)
{
  for(unsigned int i=0; i < num_joints_;++i)
  {
   joint_position_controllers_[i]->setCommand(set_pts_[i]);
  }

  updateJointControllers();
}

void HeadPanTiltController::updateJointControllers(void)
{
  for(unsigned int i=0;i<num_joints_;++i)
    joint_position_controllers_[i]->update();
}

//------ Head controller node --------

ROS_REGISTER_CONTROLLER(HeadPanTiltControllerNode)

HeadPanTiltControllerNode::HeadPanTiltControllerNode()
  : Controller(), TF(*ros::node::instance(),false)
{
  c_ = new HeadPanTiltController();
  node = ros::node::instance();  
  std::cout<<"Controller node created"<<endl;
  
}

HeadPanTiltControllerNode::~HeadPanTiltControllerNode()
{
  node->unadvertise_service(service_prefix + "/set_command_array");
  node->unadvertise_service(service_prefix + "/get_command_array");

  delete c_;
}

void HeadPanTiltControllerNode::update()
{
  c_->update();
}

bool HeadPanTiltControllerNode::initXml(mechanism::RobotState * robot, TiXmlElement * config)
{
  std::cout<<"LOADING HEAD PAN TILT CONTROLLER NODE"<<std::endl;
  service_prefix = config->Attribute("name");
  std::cout<<"the prefix is "<<service_prefix<<std::endl;
  
  // Parses subcontroller configuration
  if(c_->initXml(robot, config))
  {
    node->advertise_service(service_prefix + "/set_command_array", &HeadPanTiltControllerNode::setJointCmd, this);
    node->advertise_service(service_prefix + "/get_command_array", &HeadPanTiltControllerNode::getJointCmd, this);
    node->advertise_service(service_prefix + "/track_point", &HeadPanTiltControllerNode::trackPoint, this);

    return true;
  }
  return false;
}

bool HeadPanTiltControllerNode::setJointCmd(pr2_mechanism_controllers::SetJointCmd::request &req,
                                   pr2_mechanism_controllers::SetJointCmd::response &resp)
{
  std::vector<double> pos;
  std::vector<std::string> names;
  pr2_mechanism_controllers::JointCmd cmds;
  req.get_positions_vec(pos);
  req.get_names_vec(names);

  c_->setJointCmd(pos,names);
  c_->getJointCmd(cmds);
  resp.set_positions_vec(pos);
  resp.set_names_vec(cmds.names);
  
  return true;
}

bool HeadPanTiltControllerNode::getJointCmd(pr2_mechanism_controllers::GetJointCmd::request &req,
                    pr2_mechanism_controllers::GetJointCmd::response &resp)
{
  pr2_mechanism_controllers::JointCmd cmd;
  c_->getJointCmd(cmd);
  resp.command = cmd;
  return true;
}

bool HeadPanTiltControllerNode::trackPoint(pr2_mechanism_controllers::TrackPoint::request &req, pr2_mechanism_controllers::TrackPoint::response &resp)
{
  std::vector<double> pos;
  std::vector<std::string> names;

  libTF::TFPoint point;
  point.x = req.target.point.x;
  point.y = req.target.point.y;
  point.z = req.target.point.z;
  point.time = req.target.header.stamp.toNSec();
  point.frame = req.target.header.frame_id;
  
  libTF::TFPoint pan_point = TF.transformPoint("head_pan",point);
  int id = c_->getJointControllerByName("head_pan_joint");
  assert(id>=0);
  double mes_pan_angle = c_->joint_position_controllers_[id]->getMeasuredPosition();
  double head_pan_angle= mes_pan_angle + atan2(pan_point.y, pan_point.x); 
  
  names.push_back("head_pan_joint");
  pos.push_back(head_pan_angle);
  
  libTF::TFPoint tilt_point = TF.transformPoint("head_tilt",point);
  id = c_->getJointControllerByName("head_tilt_joint");
  assert(id>=0);
  double mes_tilt_angle= c_->joint_position_controllers_[id]->getMeasuredPosition();
  double head_tilt_angle= mes_tilt_angle + atan2(-tilt_point.z, tilt_point.x);
  
  names.push_back("head_tilt_joint");
  pos.push_back(head_tilt_angle);
  
     
  c_->setJointCmd(pos,names);
  resp.pan_angle = head_pan_angle;
  resp.tilt_angle =head_tilt_angle;
  return true;
}



