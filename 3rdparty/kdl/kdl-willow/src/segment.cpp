// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#include "segment.hpp"
#include <stdio.h>

namespace KDL {

    Segment::Segment(const Joint& _joint, const Frame& _f_tip, const Inertia& _M, const Vector& _r_cm):
        joint(_joint),M(_M),
        f_tip(_joint.pose(0).Inverse() * _f_tip),r_cm(_r_cm)
    {
    }

    Segment::Segment(const Segment& in):
        joint(in.joint),M(in.M),
        f_tip(in.f_tip),r_cm(in.r_cm)
    {
    }

    Segment& Segment::operator=(const Segment& arg)
    {
        joint=arg.joint;
        M=arg.M;
        f_tip=arg.f_tip;
				r_cm=arg.r_cm;
        return *this;
    }

    Segment::~Segment()
    {
    }

    Frame Segment::pose(const double& q)const
    {
        return joint.pose(q)*f_tip;
    }

    Twist Segment::twist(const double& q, const double& qdot)const
    {
      return joint.twist(qdot).RefPoint(joint.pose(q).M * f_tip.p);
    }

    Wrench Segment::wrench(const double& q, const double& eff)const
    {
        return joint.wrench(eff).RefPoint(pose(q).p);
    }

}//end of namespace KDL

