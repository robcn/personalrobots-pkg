/********************************************************************* * Software License Agreement (BSD License)
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

#include "ros/node.h"

// Services
#include "point_cloud_assembler/BuildCloud.h"
#include "point_cloud_assembler/BuildCloudAngle.h"
#include "pr2_srvs/SetPeriodicCmd.h"

// Messages
#include "sensor_msgs/PointCloud.h"
#include "pr2_msgs/LaserScannerSignal.h"

#include <boost/thread/mutex.hpp>


/***
 * This uses the point_cloud_assembler's build_cloud service call to grab all the scans/clouds between two tilt-laser shutters
 * params
 *  * "~target_frame_id" (string) - This is the frame that the scanned data transformed into.  The
 *                                  output clouds are also published in this frame.
 */

namespace point_cloud_assembler
{

using namespace ros;
using namespace std;

class PointCloudSrv : public ros::Node
{
private:
  pr2_msgs::LaserScannerSignal laser_scanner_signal_;
  ros::Time laser_time_;
  boost::mutex laser_mutex_;


public:
  PointCloudSrv() : 
    ros::Node("point_cloud_srv")
  {
    advertiseService("point_cloud_srv/single_sweep_cloud", &PointCloudSrv::buildSingleSweepCloud, this);
    subscribe("laser_tilt_controller/laser_scanner_signal", laser_scanner_signal_, &PointCloudSrv::scannerSignalCallback, 40) ;

    laser_time_ = Time().fromSec(0);
  }

  ~PointCloudSrv()
  {
    unsubscribe("laser_tilt_controller/laser_scanner_signal") ;
    unadvertiseService("point_cloud_srv/single_sweep_cloud") ;
  }


  bool buildSingleSweepCloud(point_cloud_assembler::BuildCloudAngle::Request &req, 
                             point_cloud_assembler::BuildCloudAngle::Response &res)
  {
    // send command to tilt laser scanner
    pr2_srvs::SetPeriodicCmd::Request scan_req;
    pr2_srvs::SetPeriodicCmd::Response scan_res;
    scan_req.command.amplitude  = fabs(req.angle_end - req.angle_begin)/2.0;
    scan_req.command.offset = (req.angle_end + req.angle_begin)/2.0;
    scan_req.command.period = req.duration*2.0;
    scan_req.command.profile = "linear";
    if (!ros::service::call("laser_tilt_controller/set_periodic_cmd", scan_req, scan_res))
      ROS_ERROR("PointCloudSrv: error setting laser scanner periodic command");
    else
      ROS_INFO("PointCloudSrv: commanded tilt laser scanner with period %f, amplitude %f and offset %f",
	       scan_req.command.period, scan_req.command.amplitude, scan_req.command.offset);


    // wait for signal from laser to know when scan is finished
    Time begin_time = scan_res.start_time;
    Duration timeout = Duration().fromSec(2.0);
    while (laser_time_ < begin_time){
      boost::mutex::scoped_lock laser_lock(laser_mutex_);
      if (ros::Time::now() > begin_time + Duration().fromSec(req.duration) + timeout){
        ROS_ERROR("PointCloudSrv: Timeout waiting for laser scan to come in");
        return false;
      }
      laser_lock.unlock();
      Duration().fromSec(0.05).sleep();
    }
    Time end_time = laser_time_;
    ROS_INFO("PointCloudSrv: generated point cloud from time %f to %f", begin_time.toSec(), end_time.toSec());

    // get a point cloud from the point cloud assembler
    BuildCloud::Request assembler_req ;
    BuildCloud::Response assembler_res ;
    assembler_req.begin = begin_time;
    assembler_req.end   = end_time;
    if (!ros::service::call("laser_scan_assembler/build_cloud", assembler_req, assembler_res))
      ROS_ERROR("PointCloudSrv: error receiving point cloud from point cloud assembler");
    else
      ROS_INFO("PointCloudSrv: received point cloud of size %i from point cloud assembler", assembler_res.cloud.points.size());

    res.cloud = assembler_res.cloud;
    return true;
  }


  void scannerSignalCallback()
  {
    boost::mutex::scoped_lock laser_lock(laser_mutex_);
    // note time when tilt laser is pointing down
    if (laser_scanner_signal_.signal == 1){
      laser_time_ = laser_scanner_signal_.header.stamp;
    }
}

} ;

}


using namespace point_cloud_assembler ;

int main(int argc, char **argv)
{
  ros::init(argc, argv);
  PointCloudSrv cloud_srv;
  cloud_srv.spin();
  
  return 0;
}
