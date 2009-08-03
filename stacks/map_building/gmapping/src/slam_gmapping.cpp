/*
 * slam_gmapping
 * Copyright (c) 2008, Willow Garage, Inc.
 *
 * THE WORK (AS DEFINED BELOW) IS PROVIDED UNDER THE TERMS OF THIS CREATIVE
 * COMMONS PUBLIC LICENSE ("CCPL" OR "LICENSE"). THE WORK IS PROTECTED BY
 * COPYRIGHT AND/OR OTHER APPLICABLE LAW. ANY USE OF THE WORK OTHER THAN AS
 * AUTHORIZED UNDER THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 * 
 * BY EXERCISING ANY RIGHTS TO THE WORK PROVIDED HERE, YOU ACCEPT AND AGREE TO
 * BE BOUND BY THE TERMS OF THIS LICENSE. THE LICENSOR GRANTS YOU THE RIGHTS
 * CONTAINED HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF SUCH TERMS AND
 * CONDITIONS.
 *
 */

/* Author: Brian Gerkey */
/* Modified by: Charles DuHadway */

#include "slam_gmapping.h"

#include <iostream>

#include <time.h>

#include "ros/ros.h"
#include "ros/console.h"

#include "gmapping/sensor/sensor_range/rangesensor.h"
#include "gmapping/sensor/sensor_odometry/odometrysensor.h"

// compute linear index for given map coords
#define MAP_IDX(sx, i, j) ((sx) * (j) + (i))

SlamGMapping::SlamGMapping():
  map_to_odom_(tf::Transform(tf::Quaternion( 0, 0, 0 ), tf::Point(0, 0, 0 ))),
  laser_count_(0)
{
  // log4cxx::Logger::getLogger(ROSCONSOLE_DEFAULT_NAME)->setLevel(ros::console::g_level_lookup[ros::console::levels::Debug]);

  gsp_ = new GMapping::GridSlamProcessor(std::cerr);
  ROS_ASSERT(gsp_);

  tfB_ = new tf::TransformBroadcaster();
  ROS_ASSERT(tfB_);

  gsp_laser_ = NULL;
  gsp_odom_ = NULL;

  got_first_scan_ = false;

  // Parameters used by our GMapping wrapper
  if(!node_.getParam("~inverted_laser", inverted_laser_))
    inverted_laser_ = false;
  if(!node_.getParam("~throttle_scans", throttle_scans_))
    throttle_scans_ = 1;
  if(!node_.getParam("~base_frame", base_frame_))
    base_frame_ = "base_link";
  if(!node_.getParam("~laser_frame", laser_frame_))
    laser_frame_ = "base_laser";
  if(!node_.getParam("~map_frame", map_frame_))
    map_frame_ = "map";
  if(!node_.getParam("~odom_frame", odom_frame_))
    odom_frame_ = "odom";

  double tmp;
  if(!node_.getParam("~map_update_interval", tmp))
    tmp = 5.0;
  map_update_interval_.fromSec(tmp);
  
  // Parameters used by GMapping itself
  if(!node_.getParam("~maxUrange", maxUrange_))
    maxUrange_ = 80.0;
  if(!node_.getParam("~sigma", sigma_))
    sigma_ = 0.05;
  if(!node_.getParam("~kernelSize", kernelSize_))
    kernelSize_ = 1;
  if(!node_.getParam("~lstep", lstep_))
    lstep_ = 0.05;
  if(!node_.getParam("~astep", astep_))
    astep_ = 0.05;
  if(!node_.getParam("~iterations", iterations_))
    iterations_ = 5;
  if(!node_.getParam("~lsigma", lsigma_))
    lsigma_ = 0.075;
  if(!node_.getParam("~ogain", ogain_))
    ogain_ = 3.0;
  if(!node_.getParam("~lskip", lskip_))
    lskip_ = 0;
  if(!node_.getParam("~srr", srr_))
    srr_ = 0.1;
  if(!node_.getParam("~srt", srt_))
    srt_ = 0.2;
  if(!node_.getParam("~str", str_))
    str_ = 0.1;
  if(!node_.getParam("~stt", stt_))
    stt_ = 0.2;
  if(!node_.getParam("~linearUpdate", linearUpdate_))
    linearUpdate_ = 1.0;
  if(!node_.getParam("~angularUpdate", angularUpdate_))
    angularUpdate_ = 0.5;
  if(!node_.getParam("~resampleThreshold", resampleThreshold_))
    resampleThreshold_ = 0.5;
  if(!node_.getParam("~particles", particles_))
    particles_ = 30;
  if(!node_.getParam("~xmin", xmin_))
    xmin_ = -100.0;
  if(!node_.getParam("~ymin", ymin_))
    ymin_ = -100.0;
  if(!node_.getParam("~xmax", xmax_))
    xmax_ = 100.0;
  if(!node_.getParam("~ymax", ymax_))
    ymax_ = 100.0;
  if(!node_.getParam("~delta", delta_))
    delta_ = 0.05;
  if(!node_.getParam("~llsamplerange", llsamplerange_))
    llsamplerange_ = 0.01;
  if(!node_.getParam("~llsamplestep", llsamplestep_))
    llsamplestep_ = 0.01;
  if(!node_.getParam("~lasamplerange", lasamplerange_))
    lasamplerange_ = 0.005;
  if(!node_.getParam("~lasamplestep", lasamplestep_))
    lasamplestep_ = 0.005;

  ss_ = node_.advertiseService("dynamic_map", &SlamGMapping::mapCallback, this);
  scan_notifier_ = new tf::MessageNotifier<sensor_msgs::LaserScan>(tf_, boost::bind(&SlamGMapping::laserCallback, this, _1), "scan", odom_frame_, 5);

  timer_ = node_.createTimer(ros::Duration(0.05), boost::bind(&SlamGMapping::publishTransform, this));
}

SlamGMapping::~SlamGMapping()
{
  delete gsp_;
  if(gsp_laser_)
    delete gsp_laser_;
  if(gsp_odom_)
    delete gsp_odom_;
  if (scan_notifier_)
    delete scan_notifier_;
}

bool
SlamGMapping::getOdomPose(GMapping::OrientedPoint& gmap_pose, const ros::Time& t)
{
  // Get the robot's pose
  tf::Stamped<tf::Pose> ident (btTransform(btQuaternion(0,0,0),
                                           btVector3(0,0,0)), t, base_frame_);
  tf::Stamped<btTransform> odom_pose;
  try
  {
    tf_.transformPose(odom_frame_, ident, odom_pose);
  }
  catch(tf::TransformException e)
  {
    ROS_WARN("Failed to compute odom pose, skipping scan (%s)", e.what());
    return false;
  }
  double yaw,pitch,roll;
  odom_pose.getBasis().getEulerZYX(yaw, pitch, roll);

  gmap_pose = GMapping::OrientedPoint(odom_pose.getOrigin().x(),
                                      odom_pose.getOrigin().y(),
                                      yaw);
  return true;
}

bool
SlamGMapping::initMapper(const sensor_msgs::LaserScan& scan)
{
  // Get the laser's pose, relative to base.
  tf::Stamped<tf::Pose> ident;
  tf::Stamped<btTransform> laser_pose;
  ident.setIdentity();
  ident.frame_id_ = scan.header.frame_id;
  ident.stamp_ = scan.header.stamp;
  try
  {
    tf_.transformPose(base_frame_, ident, laser_pose);
  }
  catch(tf::TransformException e)
  {
    ROS_WARN("Failed to compute laser pose, aborting initialization (%s)",
             e.what());
    return false;
  }
  double yaw,pitch,roll;
  btMatrix3x3 mat =  laser_pose.getBasis();
  mat.getEulerZYX(yaw, pitch, roll);

  GMapping::OrientedPoint gmap_pose(laser_pose.getOrigin().x(),
                                    laser_pose.getOrigin().y(),
                                    yaw);
  ROS_DEBUG("laser's pose wrt base: %.3f %.3f %.3f",
            laser_pose.getOrigin().x(),
            laser_pose.getOrigin().y(),
            yaw);

  // The laser must be called "FLASER"
  gsp_laser_ = new GMapping::RangeSensor("FLASER",
                                         scan.ranges.size(),
                                         scan.angle_increment,
                                         gmap_pose,
                                         0.0,
                                         scan.range_max);
  ROS_ASSERT(gsp_laser_);

  GMapping::SensorMap smap;
  smap.insert(make_pair(gsp_laser_->getName(), gsp_laser_));
  gsp_->setSensorMap(smap);

  gsp_odom_ = new GMapping::OdometrySensor(odom_frame_);
  ROS_ASSERT(gsp_odom_);

  double maxrange = scan.range_max;

  /// @todo Expose setting an initial pose
  GMapping::OrientedPoint initialPose;
  if(!getOdomPose(initialPose, scan.header.stamp))
    initialPose = GMapping::OrientedPoint(0.0, 0.0, 0.0);

  gsp_->setMatchingParameters(maxUrange_, maxrange, sigma_,
                              kernelSize_, lstep_, astep_, iterations_,
                              lsigma_, ogain_, lskip_);

  gsp_->setMotionModelParameters(srr_, srt_, str_, stt_);
  gsp_->setUpdateDistances(linearUpdate_, angularUpdate_, resampleThreshold_);
  gsp_->setgenerateMap(false);
  gsp_->GridSlamProcessor::init(particles_, xmin_, ymin_, xmax_, ymax_,
                                delta_, initialPose);
  gsp_->setllsamplerange(llsamplerange_);
  gsp_->setllsamplestep(llsamplestep_);
  /// @todo Check these calls; in the gmapping gui, they use
  /// llsamplestep and llsamplerange intead of lasamplestep and
  /// lasamplerange.  It was probably a typo, but who knows.
  gsp_->setlasamplerange(lasamplerange_);
  gsp_->setlasamplestep(lasamplestep_);

  // Call the sampling function once to set the seed.
  GMapping::sampleGaussian(1,time(NULL));

  ROS_INFO("Initialization complete");

  return true;
}

bool
SlamGMapping::addScan(const sensor_msgs::LaserScan& scan, GMapping::OrientedPoint& gmap_pose)
{
  if(!getOdomPose(gmap_pose, scan.header.stamp))
     return false;

  // GMapping wants an array of doubles...
  double* ranges_double = new double[scan.ranges.size()];
  if (inverted_laser_) 
  {
    int num_ranges = scan.ranges.size();
    for(int i=0; i < num_ranges; i++)
    {
      // Must filter out short readings, because the mapper won't
      if(scan.ranges[i] < scan.range_min)
        ranges_double[i] = (double)scan.range_max;
      else
        ranges_double[i] = (double)scan.ranges[num_ranges - i - 1];
    }
  } else 
  {
    for(unsigned int i=0; i < scan.ranges.size(); i++)
    {
      // Must filter out short readings, because the mapper won't
      if(scan.ranges[i] < scan.range_min)
        ranges_double[i] = (double)scan.range_max;
      else
        ranges_double[i] = (double)scan.ranges[i];
    }
  }

  GMapping::RangeReading reading(scan.ranges.size(),
                                 ranges_double,
                                 gsp_laser_,
                                 scan.header.stamp.toSec());

  // ...but it deep copies them in RangeReading constructor, so we don't
  // need to keep our array around.
  delete[] ranges_double;

  reading.setPose(gmap_pose);

  /*
  ROS_DEBUG("scanpose (%.3f): %.3f %.3f %.3f\n",
            scan.header.stamp.toSec(),
            gmap_pose.x,
            gmap_pose.y,
            gmap_pose.theta);
            */

  return gsp_->processScan(reading);
}

void
SlamGMapping::laserCallback(const tf::MessageNotifier<sensor_msgs::LaserScan>::MessagePtr& scan)
{
  laser_count_++;
  if ((laser_count_ % throttle_scans_) != 0)
    return;

  static ros::Time last_map_update(0,0);

  // We can't initialize the mapper until we've got the first scan
  if(!got_first_scan_)
  {
    if(!initMapper(*scan))
      return;
    got_first_scan_ = true;
  }

  GMapping::OrientedPoint odom_pose;
  if(addScan(*scan, odom_pose))
  {
    ROS_DEBUG("scan processed");

    GMapping::OrientedPoint mpose = gsp_->getParticles()[gsp_->getBestParticleIndex()].pose;
    ROS_DEBUG("new best pose: %.3f %.3f %.3f", mpose.x, mpose.y, mpose.theta);


    getOdomPose(odom_pose, ros::Time::now());

    getOdomPose(odom_pose, scan->header.stamp);
    ROS_DEBUG("odom pose: %.3f %.3f %.3f", odom_pose.x, odom_pose.y, odom_pose.theta);
    ROS_DEBUG("correction: %.3f %.3f %.3f", mpose.x - odom_pose.x, mpose.y - odom_pose.y, mpose.theta - odom_pose.theta);

    tf::Stamped<tf::Pose> odom_to_map;
    try
    {
      tf_.transformPose(odom_frame_,tf::Stamped<tf::Pose> (btTransform(btQuaternion(mpose.theta, 0, 0),
                                                                    btVector3(mpose.x, mpose.y, 0.0)).inverse(),
                                                                    scan->header.stamp, base_frame_),odom_to_map);
    }
    catch(tf::TransformException e){
      ROS_ERROR("Transform from base_link to odom failed\n");
      odom_to_map.setIdentity();
    }

    map_to_odom_mutex_.lock();
    map_to_odom_ = tf::Transform(tf::Quaternion( odom_to_map.getRotation() ),
                                 tf::Point(      odom_to_map.getOrigin() ) ).inverse();
    map_to_odom_mutex_.unlock();

    if((scan->header.stamp - last_map_update) > map_update_interval_)
    {
      updateMap(*scan);
      last_map_update = scan->header.stamp;
      ROS_DEBUG("Updated the map");
    }
  }
}

void
SlamGMapping::updateMap(const sensor_msgs::LaserScan& scan)
{
  GMapping::ScanMatcher matcher;
  double* laser_angles = new double[scan.ranges.size()];
  double theta = scan.angle_min;
  for(unsigned int i=0; i<scan.ranges.size(); i++)
  {
    laser_angles[i]=theta;
    theta += scan.angle_increment;
  }

  /// @todo Check the pose that's being passed here
  matcher.setLaserParameters(scan.ranges.size(), laser_angles,
                             gsp_laser_->getPose());

  delete[] laser_angles;
  matcher.setlaserMaxRange(scan.range_max);
  matcher.setusableRange(maxUrange_);
  matcher.setgenerateMap(true);

  GMapping::GridSlamProcessor::Particle best =
          gsp_->getParticles()[gsp_->getBestParticleIndex()];

  /// @todo Dynamically determine bounding box for map
  GMapping::Point wmin(xmin_, ymin_);
  GMapping::Point wmax(xmax_, ymax_);
  map_.map.info.resolution = delta_;
  map_.map.info.origin.position.x = xmin_;
  map_.map.info.origin.position.y = ymin_;
  map_.map.info.origin.position.z = 0.0;
  map_.map.info.origin.orientation.x = 0.0;
  map_.map.info.origin.orientation.y = 0.0;
  map_.map.info.origin.orientation.z = 0.0;
  map_.map.info.origin.orientation.w = 1.0;

  GMapping::Point center;
  center.x=(wmin.x + wmax.x) / 2.0;
  center.y=(wmin.y + wmax.y) / 2.0;

  GMapping::ScanMatcherMap smap(center, wmin.x, wmin.y, wmax.x, wmax.y,
                                map_.map.info.resolution);

  GMapping::IntPoint imin = smap.world2map(wmin);
  GMapping::IntPoint imax = smap.world2map(wmax);
  map_.map.info.width = imax.x - imin.x;
  map_.map.info.height = imax.y - imin.y;
  map_.map.data.resize(map_.map.info.width * map_.map.info.height);

  ROS_DEBUG("Trajectory tree:");
  for(GMapping::GridSlamProcessor::TNode* n = best.node;
      n;
      n = n->parent)
  {
    ROS_DEBUG("  %.3f %.3f %.3f",
              n->pose.x,
              n->pose.y,
              n->pose.theta);
    if(!n->reading)
    {
      ROS_DEBUG("Reading is NULL");
      continue;
    }
    matcher.invalidateActiveArea();
    matcher.computeActiveArea(smap, n->pose, &((*n->reading)[0]));
    matcher.registerScan(smap, n->pose, &((*n->reading)[0]));
  }

  for(int x=0; x < smap.getMapSizeX(); x++)
  {
    for(int y=0; y < smap.getMapSizeY(); y++)
    {
      /// @todo Sort out the unknown vs. free vs. obstacle thresholding
      GMapping::IntPoint p(imin.x + x, imin.y + y);
//      double entropy = smap.cell(p).entropy();
//      int e = (int)round(entropy * 140);
//      if (e != 97)
//        printf("entropy: %f %d\n", entropy, e);
//      map_.map.data[MAP_IDX(map_.map.width, x, y)] = (int)round(entropy * 140);
      double occ=smap.cell(p);
      assert(occ <= 1.0);
      if(occ < 0)
        map_.map.data[MAP_IDX(map_.map.info.width, x, y)] = -1;
      else if(occ > 0.1)
        map_.map.data[MAP_IDX(map_.map.info.width, x, y)] = (int)round(occ*100.0);
      else
        map_.map.data[MAP_IDX(map_.map.info.width, x, y)] = 0;
    }
  }
  got_map_ = true;
}

bool 
SlamGMapping::mapCallback(nav_msgs::GetMap::Request  &req,
                          nav_msgs::GetMap::Response &res)
{
  if(got_map_ && map_.map.info.width && map_.map.info.height)
  {
    res = map_;
    return true;
  }
  else
    return false;
}

void SlamGMapping::publishTransform()
{
  map_to_odom_mutex_.lock();
  ros::Time tf_expiration = ros::Time::now() + ros::Duration(0.05);
  tfB_->sendTransform( tf::Stamped<tf::Transform> (map_to_odom_, ros::Time::now(), odom_frame_, map_frame_));
  map_to_odom_mutex_.unlock();
}