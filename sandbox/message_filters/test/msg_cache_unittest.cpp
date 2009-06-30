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

#include <gtest/gtest.h>

#include "ros/time.h"
#include "message_filters/msg_cache.h"

using namespace std ;
using namespace message_filters ;

struct Header
{
  ros::Time stamp ;
} ;


struct Msg
{
  Header header ;
  int data ;
} ;




void fill_cache_easy(MsgCache<Msg>& cache, unsigned int start, unsigned int end)
{
  for (unsigned int i=start; i < end; i++)
  {
    Msg* msg = new Msg ;
    msg->data = i ;
    msg->header.stamp.fromSec(i*10) ;

    boost::shared_ptr<Msg const> msg_ptr(msg) ;
    cache.addToCache(msg_ptr) ;
  }
}

TEST(MESSAGE_FILTERS_MSG_CACHE, easy_interval_test)
{
  MsgCache<Msg> cache(10) ;
  fill_cache_easy(cache, 0, 5) ;

  vector<boost::shared_ptr<Msg const> > interval_data = cache.getInterval(ros::Time().fromSec(5), ros::Time().fromSec(35)) ;

  EXPECT_EQ(interval_data.size(), (unsigned int) 3) ;
  EXPECT_EQ(interval_data[0]->data, 1) ;
  EXPECT_EQ(interval_data[1]->data, 2) ;
  EXPECT_EQ(interval_data[2]->data, 3) ;

  // Look for an interval past the end of the cache
  interval_data = cache.getInterval(ros::Time().fromSec(55), ros::Time().fromSec(65)) ;
  EXPECT_EQ(interval_data.size(), (unsigned int) 0) ;

  // Look for an interval that fell off the back of the cache
  fill_cache_easy(cache, 5, 20) ;
  interval_data = cache.getInterval(ros::Time().fromSec(5), ros::Time().fromSec(35)) ;
  EXPECT_EQ(interval_data.size(), (unsigned int) 0) ;
}

TEST(MESSAGE_FILTERS_MSG_CACHE, easy_elem_before_after)
{
  MsgCache<Msg> cache(10) ;
  boost::shared_ptr<Msg const> elem_ptr ;

  fill_cache_easy(cache, 5, 10) ;

  elem_ptr = cache.getElemAfterTime( ros::Time().fromSec(85.0)) ;

  ASSERT_FALSE(!elem_ptr) ;
  EXPECT_EQ(elem_ptr->data, 9) ;

  elem_ptr = cache.getElemBeforeTime( ros::Time().fromSec(85.0)) ;
  ASSERT_FALSE(!elem_ptr) ;
  EXPECT_EQ(elem_ptr->data, 8) ;

  elem_ptr = cache.getElemBeforeTime( ros::Time().fromSec(45.0)) ;
  EXPECT_TRUE(!elem_ptr) ;
}



int main(int argc, char **argv){
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
