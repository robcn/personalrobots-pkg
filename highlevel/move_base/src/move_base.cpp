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
* Author: Eitan Marder-Eppstein
*********************************************************************/
#include <move_base/move_base.h>
#include <cstdlib>
#include <ctime>

using namespace base_local_planner;
using namespace costmap_2d;
using namespace navfn;
using namespace robot_actions;

namespace move_base {

  double sign(double x){
    return x < 0.0 ? -1.0 : 1.0;
  }

  MoveBase::MoveBase(ros::Node& ros_node, tf::TransformListener& tf) : 
    Action<robot_msgs::PoseStamped, robot_msgs::PoseStamped>(ros_node.getName()), ros_node_(ros_node), tf_(tf),
    run_planner_(true), tc_(NULL), planner_costmap_ros_(NULL), controller_costmap_ros_(NULL), 
    planner_(NULL), valid_plan_(false), new_plan_(false), attempted_rotation_(false), attempted_costmap_reset_(false),
    done_half_rotation_(false), done_full_rotation_(false), escaping_(false) {

    //get some parameters that will be global to the move base node
    ros_node_.param("~navfn/costmap/robot_base_frame", robot_base_frame_, std::string("base_link"));
    ros_node_.param("~navfn/costmap/global_frame", global_frame_, std::string("/map"));
    ros_node_.param("~controller_frequency", controller_frequency_, 20.0);
    ros_node_.param("~planner_patience", planner_patience_, 5.0);
    ros_node_.param("~controller_patience", controller_patience_, 15.0);

    //for comanding the base
    ros_node_.advertise<robot_msgs::PoseDot>("cmd_vel", 1);
    ros_node_.advertise<visualization_msgs::Marker>( "visualization_marker", 0 );

    //we'll assume the radius of the robot to be consistent with what's specified for the costmaps
    ros_node_.param("~base_local_planner/costmap/inscribed_radius", inscribed_radius_, 0.325);
    ros_node_.param("~base_local_planner/costmap/circumscribed_radius", circumscribed_radius_, 0.46);
    ros_node_.param("~clearing_radius", clearing_radius_, circumscribed_radius_);

    robot_msgs::Point pt;
    double padding;
    ros_node_.param("~footprint_padding", padding, 0.01);

    //grab the footprint from the parameter server if possible
    XmlRpc::XmlRpcValue footprint_list;
    if(ros_node_.getParam("~footprint", footprint_list)){
      //make sure we have a list of lists
      ROS_ASSERT_MSG(footprint_list.getType() == XmlRpcValue::TypeArray && footprint_list.size() > 2, 
          "The footprint must be specified as list of lists on the parameter server eg: [[x1, y1], [x2, y2], ..., [xn, yn]]");
      for(int i = 0; i < footprint_list.size(); ++i){
        //make sure we have a list of lists of size 2
        XmlRpc::XmlRpcValue point = footprint_list[i];
        ROS_ASSERT_MSG(point.getType() == XmlRpc::XmlRpcValue::TypeArray && point.size() == 2, 
            "The footprint must be specified as list of lists on the parameter server eg: [[x1, y1], [x2, y2], ..., [xn, yn]]");

        //make sure that the value we're looking at is either a double or an int
        ROS_ASSERT(point[0].getType() == XmlRpc::XmlRpcValue::TypeInt || point[0].getType() == XmlRpc::XmlRpcValue::TypeDouble);
        pt.x = point[0].getType() == XmlRpc::XmlRpcValue::TypeInt ? (int)(point[0]) : (double)(point[0]);
        pt.x += sign(pt.x) * padding;

        //make sure that the value we're looking at is either a double or an int
        ROS_ASSERT(point[1].getType() == XmlRpc::XmlRpcValue::TypeInt || point[1].getType() == XmlRpc::XmlRpcValue::TypeDouble);
        pt.y = point[1].getType() == XmlRpc::XmlRpcValue::TypeInt ? (int)(point[1]) : (double)(point[1]);
        pt.y += sign(pt.y) * padding;

        footprint_.push_back(pt);
        
      }
    }
    else{
      //if no explicit footprint is set on the param server... create a square footprint
      pt.x = inscribed_radius_ + padding;
      pt.y = -1 * (inscribed_radius_ + padding);
      footprint_.push_back(pt);
      pt.x = -1 * (inscribed_radius_ + padding);
      pt.y = -1 * (inscribed_radius_ + padding);
      footprint_.push_back(pt);
      pt.x = -1 * (inscribed_radius_ + padding);
      pt.y = inscribed_radius_ + padding;
      footprint_.push_back(pt);
      pt.x = inscribed_radius_ + padding;
      pt.y = inscribed_radius_ + padding;
      footprint_.push_back(pt);

      //give the robot a nose
      pt.x = circumscribed_radius_;
      pt.y = 0;
      footprint_.push_back(pt);
    }

    //create the ros wrapper for the planner's costmap... and initializer a pointer we'll use with the underlying map
    planner_costmap_ros_ = new Costmap2DROS(ros_node_, tf_, std::string("navfn"), footprint_);
    planner_costmap_ros_->getCostmapCopy(planner_costmap_);

    //initialize the NavFn planner
    planner_ = new NavfnROS(ros_node_, tf_, planner_costmap_);
    ROS_INFO("MAP SIZE: %d, %d", planner_costmap_.cellSizeX(), planner_costmap_.cellSizeY());

    //create the ros wrapper for the controller's costmap... and initializer a pointer we'll use with the underlying map
    controller_costmap_ros_ = new Costmap2DROS(ros_node_, tf_, std::string("base_local_planner"), footprint_);
    controller_costmap_ros_->getCostmapCopy(controller_costmap_);

    //create a trajectory controller
    tc_ = new TrajectoryPlannerROS(ros_node_, tf_, controller_costmap_, footprint_);

    //advertise a service for getting a plan
    ros_node_.advertiseService("~make_plan", &MoveBase::planService, this);

    //initially clear any unknown space around the robot
    planner_costmap_ros_->clearNonLethalWindow(circumscribed_radius_ * 2, circumscribed_radius_ * 2);
    controller_costmap_ros_->clearNonLethalWindow(circumscribed_radius_ * 2, circumscribed_radius_ * 2);

    //TODO:spawn planning thread here?
  }

  void MoveBase::clearCostmapWindows(double size_x, double size_y){
    tf::Stamped<tf::Pose> global_pose;

    //clear the planner's costmap
    getRobotPose(planner_costmap_ros_->globalFrame(), global_pose);

    std::vector<robot_msgs::Point> clear_poly;
    double x = global_pose.getOrigin().x();
    double y = global_pose.getOrigin().y();
    robot_msgs::Point pt;

    pt.x = x - size_x / 2;
    pt.y = y - size_x / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y - size_x / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y + size_x / 2;
    clear_poly.push_back(pt);

    pt.x = x - size_x / 2;
    pt.y = y + size_x / 2;
    clear_poly.push_back(pt);

    planner_costmap_ros_->setConvexPolygonCost(clear_poly, costmap_2d::FREE_SPACE);

    //clear the controller's costmap
    getRobotPose(controller_costmap_ros_->globalFrame(), global_pose);

    clear_poly.clear();
    x = global_pose.getOrigin().x();
    y = global_pose.getOrigin().y();

    pt.x = x - size_x / 2;
    pt.y = y - size_x / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y - size_x / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y + size_x / 2;
    clear_poly.push_back(pt);

    pt.x = x - size_x / 2;
    pt.y = y + size_x / 2;
    clear_poly.push_back(pt);

    controller_costmap_ros_->setConvexPolygonCost(clear_poly, costmap_2d::FREE_SPACE);
  }

  bool MoveBase::planService(nav_srvs::Plan::Request &req, nav_srvs::Plan::Response &resp){
    if(isActive()){
      ROS_ERROR("move_base must be in an inactive state to make a plan for an external user");
      return false;
    }

    //make sure we have a costmap for our planner
    if(planner_costmap_ros_ == NULL){
      ROS_ERROR("move_base cannot make a plan for you because it doesn't have a costmap");
      return false;
    }

    //update the copy of the costmap the planner uses
    clearCostmapWindows(2 * clearing_radius_, 2 * clearing_radius_);
    planner_costmap_ros_->clearRobotFootprint();
    planner_costmap_ros_->getCostmapCopy(planner_costmap_);

    //since we have a controller that knows the full footprint of the robot... we may as well clear it
    //tc_->clearRobotFootprint(planner_costmap_); //now done in sensors

    //if we have a tolerance on the goal point that is greater 
    //than the resolution of the map... compute the full potential function
    double resolution = planner_costmap_ros_->resolution();
    std::vector<robot_msgs::PoseStamped> global_plan;
    robot_msgs::PoseStamped p;
    p = req.goal;
    p.pose.position.y = req.goal.pose.position.y - req.tolerance; 
    bool found_legal = false;
    while(!found_legal && p.pose.position.y <= req.goal.pose.position.y + req.tolerance){
      p.pose.position.x = req.goal.pose.position.x - req.tolerance;
      while(!found_legal && p.pose.position.x <= req.goal.pose.position.x + req.tolerance){
        if(planner_->makePlan(p, global_plan)){
          if(!global_plan.empty()){
            global_plan.push_back(p);
            found_legal = true;
          }
          else
            ROS_DEBUG("Failed to find a  plan to point (%.2f, %.2f)", p.pose.position.x, p.pose.position.y);
        }
        p.pose.position.x += resolution*3.0;
      }
      p.pose.position.y += resolution*3.0;
    }

    //copy the plan into a message to send out
    resp.plan.set_poses_size(global_plan.size());
    for(unsigned int i = 0; i < global_plan.size(); ++i){
      resp.plan.poses[i] = global_plan[i];
    }



    return true;
  }

  MoveBase::~MoveBase(){
    if(planner_ != NULL)
      delete planner_;

    if(tc_ != NULL)
      delete tc_;

    if(planner_costmap_ros_ != NULL)
      delete planner_costmap_ros_;

    if(controller_costmap_ros_ != NULL)
      delete controller_costmap_ros_;
  }

  void MoveBase::publishGoal(const robot_msgs::PoseStamped& goal){
    visualization_msgs::Marker marker;
    marker.header = goal.header;
    marker.ns = "move_base";
    marker.id = 0;
    marker.type = visualization_msgs::Marker::ARROW;
    marker.pose = goal.pose;
    marker.scale.x = 0.5;
    marker.scale.y = 0.4;
    marker.scale.z = 0.4;
    marker.color.a = 1.0;
    marker.color.r = 0.0;
    marker.color.g = 1.0;
    marker.color.b = 0.0;
    ros_node_.publish("visualization_marker", marker);
  }

  void MoveBase::makePlan(const robot_msgs::PoseStamped& goal){
    //since this gets called on handle activate
    if(planner_costmap_ros_ == NULL)
      return;

    //update the copy of the costmap the planner uses
    planner_costmap_ros_->clearRobotFootprint();
    planner_costmap_ros_->getCostmapCopy(planner_costmap_);

    //since we have a controller that knows the full footprint of the robot... we may as well clear it
    //tc_->clearRobotFootprint(planner_costmap_); //now done in sensors

    std::vector<robot_msgs::PoseStamped> global_plan;
    bool valid_plan = planner_->makePlan(goal, global_plan);

    //sometimes the planner returns zero length plans and reports success
    if(global_plan.empty()){
      valid_plan = false;
    }


    //we'll also push the goal point onto the end of the plan to make sure orientation is taken into account
    if(valid_plan){
      robot_msgs::PoseStamped goal_copy = goal;
      goal_copy.header.stamp = ros::Time::now();
      global_plan.push_back(goal_copy);

      //reset our flags for attempts to help create a valid plan
      attempted_rotation_ = false;
      attempted_costmap_reset_ = false;
      new_plan_ = true;
    }
    else{
      ROS_DEBUG("Failed to find a  plan to point (%.2f, %.2f)", goal.pose.position.x, goal.pose.position.y);
    }

    lock_.lock();
    //copy over the new global plan
    valid_plan_ = valid_plan;
    global_plan_ = global_plan;
    lock_.unlock();
  }

  void MoveBase::getRobotPose(std::string frame, tf::Stamped<tf::Pose>& pose){
    tf::Stamped<tf::Pose> robot_pose;
    robot_pose.setIdentity();
    robot_pose.frame_id_ = robot_base_frame_;
    robot_pose.stamp_ = ros::Time();

    try{
      tf_.transformPose(frame, robot_pose, pose);
    }
    catch(tf::LookupException& ex) {
      ROS_ERROR("No Transform available Error: %s\n", ex.what());
      return;
    }
    catch(tf::ConnectivityException& ex) {
      ROS_ERROR("Connectivity Error: %s\n", ex.what());
      return;
    }
    catch(tf::ExtrapolationException& ex) {
      ROS_ERROR("Extrapolation Error: %s\n", ex.what());
    }
  }

  robot_actions::ResultStatus MoveBase::execute(const robot_msgs::PoseStamped& goal, robot_msgs::PoseStamped& feedback){
    //on activation... we'll reset our costmaps
    clearCostmapWindows(2 * clearing_radius_, 2 * clearing_radius_);

    //publish the goal point to the visualizer
    publishGoal(goal);

    //update the goal
    goal_ = goal;

    //first... make a plan to the goal
    makePlan(goal_);

    costmap_2d::Rate r(controller_frequency_);
    last_valid_control_ = ros::Time::now();
    robot_msgs::PoseDot cmd_vel;
    while(!isPreemptRequested() && ros_node_.ok()){
      struct timeval start, end;
      double start_t, end_t, t_diff;
      gettimeofday(&start, NULL);

      //update feedback to correspond to our current position
      tf::Stamped<tf::Pose> global_pose;
      getRobotPose(goal_.header.frame_id, global_pose);
      tf::PoseStampedTFToMsg(global_pose, feedback);

      //push the feedback out
      update(feedback);

      //make sure to update the costmap we'll use for this cycle
      controller_costmap_ros_->clearRobotFootprint();
      controller_costmap_ros_->getCostmapCopy(controller_costmap_);

      //check that the observation buffers for the costmap are current
      if(!controller_costmap_ros_->isCurrent()){
        ROS_WARN("Sensor data is out of date, we're not going to allow commanding of the base for safety");
        cmd_vel.vel.vx = 0.0;
        cmd_vel.vel.vy = 0.0;
        cmd_vel.ang_vel.vz = 0.0;
        //give the base the velocity command
        ros_node_.publish("cmd_vel", cmd_vel);
        r.sleep();
        continue;
      }


      bool valid_control = false;
      //pass plan to controller
      
      if(valid_plan_){
        //if we have a new plan... we'll update the plan for the controller
        if(new_plan_){
          new_plan_ = false;
          if(!tc_->updatePlan(global_plan_)){
            resetState();
            ROS_WARN("move_base aborted because it failed to pass the plan from the planner to the controller");
            return robot_actions::ABORTED;
          }
        }

        //compute velocity commands to send to the base
        valid_control = tc_->computeVelocityCommands(cmd_vel);
        ROS_DEBUG("Velocity commands produced by controller: vx: %.2f, vy: %.2f, vth: %.2f", cmd_vel.vel.vx, cmd_vel.vel.vy, cmd_vel.ang_vel.vz);

        if(valid_control)
          last_valid_control_ = ros::Time::now();

        //check for success
        if(tc_->goalReached()){
          if(attempted_rotation_){
            valid_control = false;
            if(done_half_rotation_){
              done_full_rotation_ = true;
            }
            else{
              done_half_rotation_ = true;
            }
          }
          else if(escaping_){
            resetState();
            valid_control = false;
          }
          else
            return robot_actions::SUCCESS;
        }

        //if we can't rotate to clear out space... just say we've done them and try to reset to the static map
        if(!valid_control && attempted_rotation_){
          done_full_rotation_ = true;
          done_half_rotation_ = true;
        }

      }
      else{
        //we don't have a valid plan... so we want to stop
        cmd_vel.vel.vx = 0.0;
        cmd_vel.vel.vy = 0.0;
        cmd_vel.ang_vel.vz = 0.0;
      }

      //give the base the velocity command
      ros_node_.publish("cmd_vel", cmd_vel);

      //if we don't have a valid control... we need to re-plan explicitly
      if(!valid_control){
        ros::Duration patience = ros::Duration(controller_patience_);

        //if we have a valid plan, but can't find a valid control for a certain time... abort
        ROS_DEBUG("Last valid control was %.2f seconds ago", (ros::Time::now() - last_valid_control_).toSec());
        if(last_valid_control_ + patience < ros::Time::now()){
          if(attempted_rotation_){
            ROS_INFO("Attempting aggresive reset of costmaps because we can't rotate");
            resetCostmaps(circumscribed_radius_ * 2, circumscribed_radius_ * 2);
            done_half_rotation_ = true;
            done_full_rotation_ = true;
          }
          else{
            resetState();
            resetCostmaps(circumscribed_radius_ * 2, circumscribed_radius_ * 2);
            ROS_WARN("move_base aborting because the controller could not find valid velocity commands for over %.4f seconds", patience.toSec());
            return robot_actions::ABORTED;
          }
        }

        //try to make a plan
        if((done_half_rotation_ && !done_full_rotation_) || !tryPlan(goal_)){
          last_valid_control_ = ros::Time::now();
          //if we've tried to reset our map and to rotate in place, to no avail, we'll abort the goal
          if(attempted_costmap_reset_ && done_full_rotation_){
            resetState();
            ROS_WARN("move_base aborting because the planner could not find a valid plan, even after reseting the map and attempting in place rotation");
            return robot_actions::ABORTED;
          }

          if(done_full_rotation_){
            ROS_INFO("Done one full rotation, resetting costmaps aggresively");
            resetCostmaps(circumscribed_radius_ * 2, circumscribed_radius_ * 2);
            attempted_rotation_ = false;
            done_half_rotation_ = false;
            done_full_rotation_ = false;
            attempted_costmap_reset_ = true;
          }
          else{
            ROS_INFO("Setting new rotation goal and resetting costmaps outside of 3 meter window");
            //clear things in the static map that are really far away
            resetCostmaps(3.0, 3.0);
            //if planning fails... we'll try rotating in place to clear things out
            double angle = M_PI; //rotate 180 degrees
            tf::Stamped<tf::Pose> rotate_goal = tf::Stamped<tf::Pose>(tf::Pose(tf::Quaternion(angle, 0.0, 0.0), tf::Point(0.0, 0.0, 0.0)), ros::Time(), robot_base_frame_);
            robot_msgs::PoseStamped rotate_goal_msg;

            try{
              tf_.transformPose(global_frame_, rotate_goal, rotate_goal);
            }
            catch(tf::TransformException& ex){
              ROS_ERROR("This tf error should never happen, %s", ex.what());
              return robot_actions::ABORTED;
              
            }

            PoseStampedTFToMsg(rotate_goal, rotate_goal_msg);
            global_plan_.clear();
            global_plan_.push_back(rotate_goal_msg);
            valid_plan_ = true;
            new_plan_ = true;
            attempted_rotation_ = true;
          }
        }

        r.sleep();
        continue;
      }

      gettimeofday(&end, NULL);
      start_t = start.tv_sec + double(start.tv_usec) / 1e6;
      end_t = end.tv_sec + double(end.tv_usec) / 1e6;
      t_diff = end_t - start_t;
      ROS_DEBUG("Full control cycle: %.9f Valid control: %d, Vel Cmd (%.2f, %.2f, %.2f)", t_diff, valid_control, cmd_vel.vel.vx, cmd_vel.vel.vy, cmd_vel.ang_vel.vz);

      //sleep the remainder of the cycle
      if(!r.sleep())
        ROS_WARN("Control loop missed its desired rate of %.4fHz... the loop actually took %.4f seconds", controller_frequency_, r.cycleTime().toSec());
    }

    //make sure to stop on pre-emption
    cmd_vel.vel.vx = 0.0;
    cmd_vel.vel.vy = 0.0;
    cmd_vel.ang_vel.vz = 0.0;
    //give the base the velocity command
    ros_node_.publish("cmd_vel", cmd_vel);
    return robot_actions::PREEMPTED;
  }

  void MoveBase::resetState(){
    attempted_rotation_ = false;
    done_half_rotation_ = false;
    done_full_rotation_ = false;
    attempted_costmap_reset_ = false;
    escaping_ = false;
  }

  bool MoveBase::tryPlan(robot_msgs::PoseStamped goal){
    ros::Duration patience = ros::Duration(planner_patience_);
    ros::Time attempt_end = ros::Time::now() + patience;
    costmap_2d::Rate r(controller_frequency_);
    while(ros::Time::now() < attempt_end && !isPreemptRequested() && ros_node_.ok()){
      makePlan(goal);

      //check if we've got a valid plan
      if(valid_plan_)
        return true;

      //for now... we'll publish zero velocity
      robot_msgs::PoseDot cmd_vel;

      last_valid_control_ = ros::Time::now();
      cmd_vel.vel.vx = 0.0;
      cmd_vel.vel.vy = 0.0;
      cmd_vel.ang_vel.vz = 0.0;
      //give the base the velocity command
      ros_node_.publish("cmd_vel", cmd_vel);

      r.sleep();
    }

    //if we still don't have a valid plan... then our planning attempt has failed
    return false;
  }

  void MoveBase::resetCostmaps(double size_x, double size_y){
    planner_costmap_ros_->resetMapOutsideWindow(size_x, size_y);
    controller_costmap_ros_->resetMapOutsideWindow(size_x, size_y);
  }

};

int main(int argc, char** argv){
  ros::init(argc, argv);
  ros::Node ros_node("move_base");
  tf::TransformListener tf(ros_node, true, ros::Duration(10));
  
  move_base::MoveBase move_base(ros_node, tf);
  robot_actions::ActionRunner runner(20.0);
  runner.connect<robot_msgs::PoseStamped, nav_robot_actions::MoveBaseState, robot_msgs::PoseStamped>(move_base);
  runner.run();

  //ros::MultiThreadedSpinner s;
  //ros::spin(s);
  
  ros_node.spin();

  return(0);

}
