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

#ifndef OGRE_VISUALIZER_LASER_SCAN_DISPLAY_H
#define OGRE_VISUALIZER_LASER_SCAN_DISPLAY_H

#include "point_cloud_base.h"
#include "helpers/color.h"
#include "ogre_tools/point_cloud.h"

#include "std_msgs/LaserScan.h"

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <deque>
#include <queue>
#include <vector>

namespace ros
{
  class node;
}

namespace tf
{
template<class Message> class MessageNotifier;
}

namespace laser_scan
{
class LaserProjection;
}

namespace ogre_vis
{

class ROSTopicStringProperty;

/**
 * \class LaserScanDisplay
 * \brief Visualizes a laser scan, received as a std_msgs::LaserScan
 */
class LaserScanDisplay : public PointCloudBase
{
public:
  LaserScanDisplay( const std::string& name, VisualizationManager* manager );
  ~LaserScanDisplay();

  // Overrides from Display
  virtual void createProperties();
  virtual void targetFrameChanged();

  /**
   * Set the incoming PointCloud topic
   * @param topic The topic we should listen to
   */
  void setTopic( const std::string& topic );
  const std::string& getTopic() { return topic_; }

  static const char* getTypeStatic() { return "Laser Scan"; }
  virtual const char* getType() { return getTypeStatic(); }
  static const char* getDescription();

protected:
  virtual void onEnable();
  virtual void onDisable();

  /**
   * \brief Subscribes to the topic set by setTopic()
   */
  void subscribe();
  /**
   * \brief Unsubscribes from the current topic
   */
  void unsubscribe();

  /**
   * \brief ROS callback for an incoming point cloud message
   */
  void incomingScanCallback(const boost::shared_ptr<std_msgs::LaserScan>& scan);

  std::string topic_;                         ///< The PointCloud topic set by setTopic()
  ROSTopicStringProperty* topic_property_;

  laser_scan::LaserProjection* projector_;

  tf::MessageNotifier<std_msgs::LaserScan>* notifier_;
};

} // namespace ogre_vis

#endif
