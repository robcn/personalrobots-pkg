#!/usr/bin/env python
# Software License Agreement (BSD License)
#
# Copyright (c) 2008, Willow Garage, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#  * Neither the name of the Willow Garage nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

import roslib
roslib.load_manifest('map_server')

import sys, time
import rospy
import std_msgs.msg
import geometry_msgs.msg
import nav_msgs.msg

import Image
import yaml
import Numeric

NAME = 'static_map_server'

class map_server:
    def __init__(self):
        self.load_time=None

    def map_handler(self, req):
        pos = geometry_msgs.Point(*tuple(self.origin))
        info=nav_msgs.msg.MapMetaData(self.load_time,
                                      self.scale,
                                      self.im.size[0],
                                      self.im.size[1],
                                      geometry_msgs.msg.Pose(position=pos),
                                      self.byte_map)
        r = nav_msgs.srv.GetMapResponse(map=nav_msgs.msg.OccupancyGrid(info=info))
        print r.map
        return r

    def start(self, args):
        self.im = Image.open(args[0]).transpose(Image.FLIP_TOP_BOTTOM)
        yaml_defs = yaml.load(open(args[1]).read())

        # Be agnostic about number of channels in image
        image_chans = [Numeric.fromstring(c.tostring(), Numeric.UnsignedInt8) for c in self.im.split()]
        def compare_color(c0, c1):
            return reduce(Numeric.logical_and, [(a == b) for (a,b) in zip(c0, c1)])

        is_vacant   = compare_color(yaml_defs['vacant'], image_chans)
        is_occupied = compare_color(yaml_defs['occupied'], image_chans)
        is_unknown  = compare_color(yaml_defs['unknown'], image_chans)

        # Everything must be one of vacant, occupied or unknown
        is_recognized = reduce(Numeric.logical_or, [is_vacant, is_occupied, is_unknown])
        if not Numeric.alltrue(is_recognized):
            # Want to tell user about the first unrecognized pixel
            offset = Numeric.nonzero(Numeric.logical_not(is_recognized))[0]
            (w,h) = self.im.size
            error_value = str([c[offset] for c in image_chans])
            error_pos = str((offset % w, (h - 1) - offset / w))
            msg = "File %s has unrecognized pixel %s at %s" % (args[0], error_value, error_pos)
            raise TypeError, msg

        # Our convention for byte map is:
        # 0     vacant
        # 100   occupied
        # 255   unknown

        rospy.init_node(NAME)

        map = Numeric.where(is_unknown, 255, Numeric.where(is_vacant, 0, 100))
        self.byte_map = map.astype(Numeric.UnsignedInt8).tostring()
        self.load_time = rospy.get_rostime() 

        self.scale = yaml_defs['resolution']
        self.origin = yaml_defs['origin']

        s1 = rospy.Service("static_map", robot_srvs.srv.StaticMap, self.map_handler)
        rospy.spin()

if __name__ == '__main__':
    try:
        if len(sys.argv) != 3:
            print >> sys.stderr, "Usage: map_server.py image yaml-def"
        map_server().start(sys.argv[1:])
    except KeyboardInterrupt, e:
        pass
    print "exiting"
