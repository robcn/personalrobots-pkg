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
#include <new_costmap/costmap_2d_ros.h>

using namespace std;

namespace costmap_2d {

  Costmap2DROS::Costmap2DROS(ros::Node& ros_node) : ros_node_(ros_node), 
  tf_(ros_node, true, ros::Duration(10)){
    ros_node_.advertise<robot_msgs::Polyline2D>("raw_obstacles", 1);
    ros_node_.advertise<robot_msgs::Polyline2D>("inflated_obstacles", 1);

    
    string topics_string;
    //get the topics that we'll subscribe to from the parameter server
    ros_node_.param("~costmap/observation_topics", topics_string, string(""));
    ROS_INFO("Topics: %s", topics_string.c_str());

    ros_node_.param("~costmap/update_frequency", freq_, 5.0);
    ros_node_.param("~costmap/global_frame", global_frame_, string("map"));

    //now we need to split the topics based on whitespace which we can use a stringstream for
    stringstream ss(topics_string);

    string topic;
    while(ss >> topic){
      //get the parameters for the specific topic
      double observation_keep_time, expected_update_rate;
      string sensor_frame, data_type;
      ros_node_.param("~costmap/" + topic + "/sensor_frame", sensor_frame, string("frame_from_message"));
      ros_node_.param("~costmap/" + topic + "/observation_persistance", observation_keep_time, 0.0);
      ros_node_.param("~costmap/" + topic + "/expected_update_rate", expected_update_rate, 0.0);
      ros_node_.param("~costmap/" + topic + "/data_type", data_type, string("PointCloud"));

      ROS_ASSERT_MSG(data_type == "PointCloud" || data_type == "LaserScan", "Only topics that use point clouds or laser scans are currently supported");

      //create an observation buffer
      observation_buffers_.push_back(new ObservationBuffer(topic, observation_keep_time, expected_update_rate, tf_, global_frame_, sensor_frame));

      ROS_INFO("Created an observation buffer for topic %s, expected update rate: %.2f, observation persistance: %.2f", topic.c_str(), expected_update_rate, observation_keep_time);

      //create a callback for the topic
      if(data_type == "LaserScan"){
        observation_notifiers_.push_back(new MessageNotifier<laser_scan::LaserScan>(&tf_, &ros_node_,
              boost::bind(&Costmap2DROS::laserScanCallback, this, _1, observation_buffers_.back()), topic, global_frame_, 50));
      }
      else{
        observation_notifiers_.push_back(new MessageNotifier<robot_msgs::PointCloud>(&tf_, &ros_node_,
              boost::bind(&Costmap2DROS::pointCloudCallback, this, _1, observation_buffers_.back()), topic, global_frame_, 50));
      }

    }


    bool static_map;
    unsigned int map_width, map_height;
    double map_resolution;
    double map_origin_x, map_origin_y;

    ros_node_.param("~costmap/static_map", static_map, true);
    std::vector<unsigned char> input_data;

    double map_width_meters, map_height_meters;
    ros_node_.param("~costmap/map_width", map_width_meters, 10.0);
    ros_node_.param("~costmap/map_height", map_height_meters, 10.0);
    ros_node_.param("~costmap/map_resolution", map_resolution, 0.05);
    ros_node_.param("~costmap/map_origin_x", map_origin_x, 0.0);
    ros_node_.param("~costmap/map_origin_y", map_origin_y, 0.0);
    map_width = (unsigned int)(map_width_meters / map_resolution);
    map_height = (unsigned int)(map_height_meters / map_resolution);

    if(static_map){
      robot_srvs::StaticMap::Request map_req;
      robot_srvs::StaticMap::Response map_resp;
      ROS_INFO("Requesting the map...\n");
      while(!ros::service::call("static_map", map_req, map_resp))
      {
        ROS_INFO("Request failed; trying again...\n");
        usleep(1000000);
      }
      ROS_INFO("Received a %d X %d map at %f m/pix\n",
          map_resp.map.width, map_resp.map.height, map_resp.map.resolution);

      //check if the user has set any parameters that will be overwritten
      bool user_map_params = false;
      user_map_params |= ros_node_.hasParam("~costmap/map_width");
      user_map_params |= ros_node_.hasParam("~costmap/map_height");
      user_map_params |= ros_node_.hasParam("~costmap/map_resolution");
      user_map_params |= ros_node_.hasParam("~costmap/map_origin_x");
      user_map_params |= ros_node_.hasParam("~costmap/map_origin_y");

      if(user_map_params)
        ROS_WARN("You have set map parameters, but also requested to use the static map. Your parameters will be overwritten by those given by the map server");

      // We are treating cells with no information as lethal obstacles based on the input data. This is not ideal but
      // our planner and controller do not reason about the no obstacle case
      unsigned int numCells = map_resp.map.width * map_resp.map.height;
      for(unsigned int i = 0; i < numCells; i++){
        input_data.push_back((unsigned char) map_resp.map.data[i]);
      }

      map_width = (unsigned int)map_resp.map.width;
      map_height = (unsigned int)map_resp.map.height;
      map_resolution = map_resp.map.resolution;
      map_origin_x = map_resp.map.origin.x;
      map_origin_y = map_resp.map.origin.y;

    }

    double inscribed_radius, circumscribed_radius, inflation_radius;
    ros_node_.param("~costmap/inscribed_radius", inscribed_radius, 0.325);
    ros_node_.param("~costmap/circumscribed_radius", circumscribed_radius, 0.46);
    ros_node_.param("~costmap/inflation_radius", inflation_radius, 0.55);

    double obstacle_range, max_obstacle_height, raytrace_range;
    ros_node_.param("~costmap/obstacle_range", obstacle_range, 2.5);
    ros_node_.param("~costmap/max_obstacle_height", max_obstacle_height, 2.0);
    ros_node_.param("~costmap/raytrace_range", raytrace_range, 3.0);

    double cost_scale;
    ros_node_.param("~costmap/cost_scaling_factor", cost_scale, 1.0);

    int temp_lethal_threshold;
    ros_node_.param("~costmap/lethal_cost_threshold", temp_lethal_threshold, int(100));

    unsigned char lethal_threshold = max(min(temp_lethal_threshold, 255), 0);

    struct timeval start, end;
    double start_t, end_t, t_diff;
    gettimeofday(&start, NULL);
    costmap_ = new Costmap2D(map_width, map_height,
        map_resolution, map_origin_x, map_origin_y, inscribed_radius, circumscribed_radius, inflation_radius, 
        obstacle_range, max_obstacle_height, raytrace_range, cost_scale, input_data, lethal_threshold);
    gettimeofday(&end, NULL);
    start_t = start.tv_sec + double(start.tv_usec) / 1e6;
    end_t = end.tv_sec + double(end.tv_usec) / 1e6;
    t_diff = end_t - start_t;
    ROS_INFO("New map construction time: %.9f", t_diff);


    //create a thread to handle updating the map
    
    //create a separate thread to publish cost data to the visualizer
    visualizer_thread_ = new boost::thread(boost::bind(&Costmap2DROS::publishCostMap, this));

    window_reset_thread_ = new boost::thread(boost::bind(&Costmap2DROS::resetWindow, this));

  }

  Costmap2DROS::~Costmap2DROS(){
    if(costmap_ != NULL)
      delete costmap_;

    if(visualizer_thread_ != NULL)
      delete visualizer_thread_;

    if(window_reset_thread_ != NULL)
      delete window_reset_thread_;

    //clean up observation buffers
    for(unsigned int i = 0; i < observation_buffers_.size(); ++i){
      if(observation_buffers_[i] != NULL)
        delete observation_buffers_[i];
    }

    //clean up message notifiers
    for(unsigned int i = 0; i < observation_notifiers_.size(); ++i){
      if(observation_notifiers_[i] != NULL)
        delete observation_notifiers_[i];
    }
  }

  void Costmap2DROS::laserScanCallback(const tf::MessageNotifier<laser_scan::LaserScan>::MessagePtr& message, ObservationBuffer* buffer){
    //project the laser into a point cloud
    PointCloud base_cloud;
    base_cloud.header = message->header;
    //we want all values... even those out of range
    projector_.projectLaser(*message, base_cloud, -1.0, true);

    //buffer the point cloud
    observation_lock_.lock();
    buffer->bufferCloud(base_cloud);
    observation_lock_.unlock();
  }

  void Costmap2DROS::pointCloudCallback(const tf::MessageNotifier<robot_msgs::PointCloud>::MessagePtr& message, ObservationBuffer* buffer){
    //buffer the point cloud
    observation_lock_.lock();
    buffer->bufferCloud(*message);
    observation_lock_.unlock();
  }

  void Costmap2DROS::spin(){
    while(ros_node_.ok()){
      updateMap();
      usleep(1e6/freq_);
    }
  }

  void Costmap2DROS::updateMap(){
    tf::Stamped<tf::Pose> robot_pose, global_pose;
    global_pose.setIdentity();
    robot_pose.setIdentity();
    robot_pose.frame_id_ = "base_link";
    robot_pose.stamp_ = ros::Time();
    try{
      tf_.transformPose(global_frame_, robot_pose, global_pose);
    }
    catch(tf::LookupException& ex) {
      ROS_ERROR("No Transform available Error: %s\n", ex.what());
    }
    catch(tf::ConnectivityException& ex) {
      ROS_ERROR("Connectivity Error: %s\n", ex.what());
    }
    catch(tf::ExtrapolationException& ex) {
      ROS_ERROR("Extrapolation Error: %s\n", ex.what());
    }

    double wx = global_pose.getOrigin().x();
    double wy = global_pose.getOrigin().y();

    bool current = true;
    std::vector<Observation> observations;
    observation_lock_.lock();
    for(unsigned int i = 0; i < observation_buffers_.size(); ++i){
      observation_buffers_[i]->getObservations(observations);
      current = current && observation_buffers_[i]->isCurrent();
    }
    observation_lock_.unlock();

    map_lock_.lock();
    struct timeval start, end;
    double start_t, end_t, t_diff;
    gettimeofday(&start, NULL);
    costmap_->updateWorld(wx, wy, observations, observations);
    gettimeofday(&end, NULL);
    start_t = start.tv_sec + double(start.tv_usec) / 1e6;
    end_t = end.tv_sec + double(end.tv_usec) / 1e6;
    t_diff = end_t - start_t;
    ROS_INFO("Map update time: %.9f", t_diff);
    map_lock_.unlock();


  }

  void Costmap2DROS::resetWindow(){
    while(ros_node_.ok()){
      tf::Stamped<tf::Pose> robot_pose, global_pose;
      global_pose.setIdentity();
      robot_pose.setIdentity();
      robot_pose.frame_id_ = "base_link";
      robot_pose.stamp_ = ros::Time();
      try{
        tf_.transformPose(global_frame_, robot_pose, global_pose);
      }
      catch(tf::LookupException& ex) {
        ROS_ERROR("No Transform available Error: %s\n", ex.what());
      }
      catch(tf::ConnectivityException& ex) {
        ROS_ERROR("Connectivity Error: %s\n", ex.what());
      }
      catch(tf::ExtrapolationException& ex) {
        ROS_ERROR("Extrapolation Error: %s\n", ex.what());
      }

      double wx = global_pose.getOrigin().x();
      double wy = global_pose.getOrigin().y();
      map_lock_.lock();
      ROS_INFO("Resetting map outside window");
      costmap_->resetMapOutsideWindow(wx, wy, 5.0, 5.0);
      map_lock_.unlock();

      usleep(1e6/0.2);
    }
  }

  void Costmap2DROS::publishCostMap(){
    while(ros_node_.ok()){
      ROS_INFO("publishing map");
      map_lock_.lock();
      std::vector< std::pair<double, double> > raw_obstacles, inflated_obstacles;
      for(unsigned int i = 0; i<costmap_->cellSizeX(); i++){
        for(unsigned int j = 0; j<costmap_->cellSizeY();j++){
          double wx, wy;
          costmap_->mapToWorld(i, j, wx, wy);
          std::pair<double, double> p(wx, wy);

          if(costmap_->getCost(i, j) == costmap_2d::LETHAL_OBSTACLE)
            raw_obstacles.push_back(p);
          else if(costmap_->getCost(i, j) == costmap_2d::INSCRIBED_INFLATED_OBSTACLE)
            inflated_obstacles.push_back(p);
        }
      }
      map_lock_.unlock();

      // First publish raw obstacles in red
      robot_msgs::Polyline2D obstacle_msg;
      obstacle_msg.header.frame_id = global_frame_;
      unsigned int pointCount = raw_obstacles.size();
      obstacle_msg.set_points_size(pointCount);
      obstacle_msg.color.a = 0.0;
      obstacle_msg.color.r = 1.0;
      obstacle_msg.color.b = 0.0;
      obstacle_msg.color.g = 0.0;

      for(unsigned int i=0;i<pointCount;i++){
        obstacle_msg.points[i].x = raw_obstacles[i].first;
        obstacle_msg.points[i].y = raw_obstacles[i].second;
      }

      ros::Node::instance()->publish("raw_obstacles", obstacle_msg);

      // Now do inflated obstacles in blue
      pointCount = inflated_obstacles.size();
      obstacle_msg.set_points_size(pointCount);
      obstacle_msg.color.a = 0.0;
      obstacle_msg.color.r = 0.0;
      obstacle_msg.color.b = 1.0;
      obstacle_msg.color.g = 0.0;

      for(unsigned int i=0;i<pointCount;i++){
        obstacle_msg.points[i].x = inflated_obstacles[i].first;
        obstacle_msg.points[i].y = inflated_obstacles[i].second;
      }

      ros::Node::instance()->publish("inflated_obstacles", obstacle_msg);
      usleep(1e6/2.0);
    }
  }

  Costmap2D Costmap2DROS::getCostMap(){
    map_lock_.lock();
    return Costmap2D(*costmap_);
    map_lock_.unlock();
  }

  unsigned char* Costmap2DROS::getCharMap(){
    map_lock_.lock();
    return costmap_->getCharMap();
    map_lock_.unlock();
  }

};

int main(int argc, char** argv){
  ros::init(argc, argv);
  ros::Node ros_node("costmap_tester");
  Costmap2DROS tester(ros_node);
  tester.spin();

  return(0);

}
