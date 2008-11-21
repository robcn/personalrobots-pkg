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
 *
 * Authors: Conor McGann
 */

#include <costmap_2d/observation_buffer.h>

namespace costmap_2d {

  // Just clean up outstanding observations
  ObservationBuffer::~ObservationBuffer(){
    while(!buffer_.empty()){
      std::list<Observation>::iterator it = buffer_.begin();
      const std_msgs::PointCloud* cloud = it->cloud_;
      delete cloud;
      buffer_.erase(it);
    }
  }

  // Only works if the observation is in the map frame - test for it. It should be transformed before
  // we enque it
  bool ObservationBuffer::buffer_observation(const Observation& observation){
    last_updated_ = observation.cloud_->header.stamp;

    if(observation.cloud_->header.frame_id != "map")
      return false;

    // If the duration is 0, then we just keep the latest one, so we clear out all existing observations
    while(!buffer_.empty()){
      std::list<Observation>::iterator it = buffer_.begin();
      // Get the current one, and check if still alive. if so
      Observation& obs = *it;
      if((last_updated_ - obs.cloud_->header.stamp) > keep_alive_){
        delete obs.cloud_;
        buffer_.erase(it);
      }
      else 
        break;
    }

    // Otherwise just store it and indicate success
    buffer_.push_back(observation);
    return true;
  }

  void ObservationBuffer::get_observations(std::vector<Observation>& observations){
    // Add all remaining observations to the output
    for(std::list<Observation>::const_iterator it = buffer_.begin(); it != buffer_.end(); ++it){
      observations.push_back(*it);
    }
  }
}
