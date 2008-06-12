//Software License Agreement (BSD License)

//Copyright (c) 2008, Willow Garage, Inc.
//All rights reserved.

//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:

// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the following
//   disclaimer in the documentation and/or other materials provided
//   with the distribution.
// * Neither the name of the Willow Garage nor the names of its
//   contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.

//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.

#ifndef PR2NEW_API_HH
#define PR2NEW_API_HH
#include <newmat10/newmat.h>
#include <libKinematics/ik.h>
#include <pr2Core/pr2Core.h>
#include <sys/types.h>
#include <stdint.h>
#include <string>

#ifdef KDL_KINEMATICS
#include <libKDL/kdl_kinematics.h> // for kinematics using KDL -- util/kinematics/libKDL
#endif

namespace PR2
{
   /*! \class 
     \brief A low-level function call based API for PR2
   */
   class PR2Robot
   {
     

         /*! \fn 
           \brief Constructor
         */
      public: PR2Robot();


         /*! \fn 
           \brief Destructor
         */
      public: virtual ~PR2Robot();


         /*! \fn
           \brief Initialize the robot, start up all interfaces
         */
      public: PR2_ERROR_CODE InitializeRobot();


         /*! \fn
           \brief Calibrate the robot
         */
      public: PR2_ERROR_CODE CalibrateRobot();


         /*! \fn
           \brief Set the joint control mode
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
           \param mode - mode for joint control, possible options are torque control, position control or speed control
         */
      public: PR2_ERROR_CODE SetJointControlMode(PR2_JOINT_ID id, PR2_JOINT_CONTROL_MODE mode);


         /*! \fn
           \brief Get the joint control mode
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
           \param mode - pointer to return value (mode for joint control, possible values are torque control, position control or speed control)
         */
      public: PR2_ERROR_CODE GetJointControlMode(PR2_JOINT_ID id, PR2_JOINT_CONTROL_MODE *mode);


         /*! \fn
           \brief Set the controller gains
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
           \param pGain - proportional gain
           \param iGain - integral gain
           \param dGain - derivative gain
         */
      public: PR2_ERROR_CODE SetJointGains(PR2_JOINT_ID id, double pGain, double iGain, double dGain);


         /*! \fn
           \brief Get the controller gains
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
           \param pGain - pointer to proportional gain
           \param iGain - pointer to integral gain
           \param dGain - pointer to derivative gain
         */
      public: PR2_ERROR_CODE GetJointGains(PR2_JOINT_ID id, double *pGain, double *iGain, double *dGain);


         /*! \fn
           \brief Enable the joint
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
         */
      public: PR2_ERROR_CODE EnableJoint(PR2_JOINT_ID id);


         /*! \fn
           \brief Disable the joint
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
         */
      public: PR2_ERROR_CODE DisableJoint(PR2_JOINT_ID id);


         /*! \fn
           \brief Return value corresponding to whether the joint is enabled or not, 0 - disabled, 1 - enabled
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs
         */
      public: PR2_ERROR_CODE IsEnabledJoint(PR2_JOINT_ID id, int *enabled);


         /*! \fn
           \brief Set particular parameters for the joint (NOT IMPLEMENTED YET)
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param pId - parameter ID corresponding to the parameter for the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param value - parameter value
         */
      public: PR2_ERROR_CODE SetJointParams(PR2_JOINT_ID id, PR2_JOINT_PARAM_ID pId, double value);


         /*! \fn
           \brief Get particular parameters for the joint  (NOT IMPLEMENTED YET)
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param pId - parameter ID corresponding to the parameter for the joint, see pr2Core.h for a list of joint IDs and parameter IDs
         */
      public: PR2_ERROR_CODE GetJointParams(PR2_JOINT_ID id, PR2_JOINT_PARAM_ID pId, double *value);


         /*! \fn
           \brief Command a desired joint position and speed 
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param jointPosition - desired joint position (in radians or meters)
           \param jointSpeed - desired joint speed (in rad/s or m/s)
         */
      public: PR2_ERROR_CODE SetJointServoCmd(PR2_JOINT_ID id, double jointPosition, double jointSpeed);


         /*! \fn
           \brief Get the commanded joint position and speed values 
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param jointPosition - pointer to desired joint position (in radians or meters)
           \param jointSpeed - desired joint speed (in rad/s or m/s)
         */
      public: PR2_ERROR_CODE GetJointServoCmd(PR2_JOINT_ID id, double *jointPosition, double *jointSpeed);

         /*! \fn
           \brief Get the actual joint position and speed values 
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param jointPosition - pointer to desired joint position (in radians or meters)
           \param jointSpeed - desired joint speed (in rad/s or m/s)
         */
      public: PR2_ERROR_CODE GetJointServoActual(PR2_JOINT_ID id, double *jointPosition, double *jointSpeed);

         /*! \fn
           \brief Get the actual joint position and speed values 
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param jointPosition - pointer to desired joint position (in radians or meters)
           \param jointSpeed - desired joint speed (in rad/s or m/s)
         */
      public: PR2_ERROR_CODE GetJointPositionActual(PR2_JOINT_ID id, double *jointPosition, double *jointSpeed);

         /*! \fn
           \brief Command a desired joint torque or force (for prismatic joints)
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param torque - desired torque (Nm or N)
         */
      public: PR2_ERROR_CODE SetJointTorque(PR2_JOINT_ID id, double torque);

         /*! \fn
           \brief Get the commanded joint torque or force (for prismatic joints)
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param torque - pointer to return value for commanded torque (Nm or N)
         */
      public: PR2_ERROR_CODE GetJointTorqueCmd(PR2_JOINT_ID id, double *torque);

         /*! \fn
           \brief Get the actual joint torque or force (for prismatic joints)
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param torque - pointer to return value for actual torque (Nm or N)
         */
      public: PR2_ERROR_CODE GetJointTorqueActual(PR2_JOINT_ID id, double *torque);

         /*! \fn
           \brief Command a desired joint speed
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param torque - desired speed (rad/s or m/s)
         */
      public: PR2_ERROR_CODE SetJointSpeed(PR2_JOINT_ID id, double speed);

         /*! \fn
           \brief Get the commanded joint speed
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param torque - desired speed (rad/s or m/s)
         */
      public: PR2_ERROR_CODE GetJointSpeedCmd(PR2_JOINT_ID id, double *speed);


         /*! \fn
           \brief Get the actual joint speed
           \param id - jointId corresponding to the joint, see pr2Core.h for a list of joint IDs and parameter IDs
           \param torque - desired speed (rad/s or m/s)
         */
      public: PR2_ERROR_CODE GetJointSpeedActual(PR2_JOINT_ID id, double *speed);


         /*! \fn
           \brief Set the control mode for the arm
           \param id - modelId corresponding to the arm, see pr2Core.h for a list of model IDs
           \param mode - two choices (joint control or cartesian control)
         */
      public: PR2_ERROR_CODE SetArmControlMode(PR2_MODEL_ID id, PR2_CONTROL_MODE mode);


         /*! \fn
           \brief Get the control mode for the arm
           \param id - modelId corresponding to the arm, see pr2Core.h for a list of model IDs
           \param mode - pointer to return value for mode, two choices (joint control or cartesian control)
         */
      public: PR2_ERROR_CODE GetArmControlMode(PR2_MODEL_ID id, PR2_CONTROL_MODE *mode);


         /*! \fn
           \brief Enable the model (i.e. enable all actuators corresponding to a particular part of the robot)
           \param id - modelID, see pr2Core.h for a list of model IDs
         */
      public: PR2_ERROR_CODE EnableModel(PR2_MODEL_ID id);


         /*! \fn
           \brief Enable the head (i.e. enable all actuators corresponding to the head)
         */
      public: PR2_ERROR_CODE EnableHead();

         /*! \fn
           \brief Enable the left gripper (i.e. enable all actuators corresponding to the left gripper)
         */
      public: PR2_ERROR_CODE EnableGripperLeft();

         /*! \fn
           \brief Enable the right gripper (i.e. enable all actuators corresponding to the right gripper)
         */
      public: PR2_ERROR_CODE EnableGripperRight();


         /*! \fn
           \brief Enable the arm (i.e. enable all actuators corresponding to an arm)
           \param id - model ID, see pr2Core.h for a list of model IDs
         */
      public: PR2_ERROR_CODE EnableArm(PR2_MODEL_ID id);


         /*! \fn
           \brief Enable the gripper (i.e. enable all actuators corresponding to the gripper)
           \param id - model ID, see pr2Core.h for a list of model IDs
         */
      public: PR2_ERROR_CODE EnableGripper(PR2_MODEL_ID id);


         /*! \fn
           \brief Enable the base (i.e. enable all actuators corresponding to the base)
         */
      public: PR2_ERROR_CODE EnableBase();


         /*! \fn
           \brief Enable the spine (i.e. enable all actuators corresponding to the spine)
         */
      public: PR2_ERROR_CODE EnableSpine();


         /*! \fn
           \brief Disable the model (i.e. disable all actuators corresponding to a particular part of the robot)
           \param id - model ID, see pr2Core.h for a list of model IDs
         */
      public: PR2_ERROR_CODE DisableModel(PR2_MODEL_ID id);


         /*! \fn
           \brief Disable the head (i.e. enable all actuators corresponding to the head)
         */
      public: PR2_ERROR_CODE DisableHead();

         /*! \fn
           \brief Disable the head (i.e. enable all actuators corresponding to the head)
         */
      public: PR2_ERROR_CODE DisableGripperLeft();

         /*! \fn
           \brief Disable the head (i.e. enable all actuators corresponding to the head)
         */
      public: PR2_ERROR_CODE DisableGripperRight();


         /*! \fn
           \brief Disable the arm (i.e. disable all actuators corresponding to an arm)
           \param id - model ID, see pr2Core.h for a list of model IDs
         */
      public: PR2_ERROR_CODE DisableArm(PR2_MODEL_ID id);


         /*! \fn
           \brief Disable the gripper (i.e. disable all actuators corresponding to a gripper)
           \param id - model ID, see pr2Core.h for a list of model IDs
         */
      public: PR2_ERROR_CODE DisableGripper(PR2_MODEL_ID id);


         /*! \fn
           \brief Disable the base (i.e. disable all actuators corresponding to the base)
         */
      public: PR2_ERROR_CODE DisableBase();


         /*! \fn
           \brief Disable the spine (i.e. disable all actuators corresponding to the spine)
         */
      public: PR2_ERROR_CODE DisableSpine();


         /*! \fn
           \brief Check whether all actuators in a particular part of the robot have been enabled
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param enabled - pointer to return value - 0 if any actuator in that part of the robot is disabled, 1 if all actuators in that part of the robot are enabled 
         */
      public: PR2_ERROR_CODE IsEnabledModel(PR2_MODEL_ID id, int *enabled);    


         /*! \fn
           \brief Command the arm to go to a particular position in joint space
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param jointPosition - array of desired positions of all the joints
           \param jointSpeed - array of desired speeds of all the joints
         */
      public: PR2_ERROR_CODE SetArmJointPosition(PR2_MODEL_ID id, double jointPosition[], double jointSpeed[]);


         /*! \fn
           \brief Get the commanded joint values for an arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param jointPosition - array of desired positions of all the joints
           \param jointSpeed - array of desired speeds of all the joints
         */
      public: PR2_ERROR_CODE GetArmJointPositionCmd(PR2_MODEL_ID id, double jointPosition[], double jointSpeed[]);


         /*! \fn
           \brief Get the actual wrist pose 
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param *x pointer to return value of x position of the wrist 
           \param *y pointer to return value of y position of the wrist 
           \param *z pointer to return value of z position of the wrist
           \param *roll pointer to return value of roll of the wrist 
           \param *pitch pointer to return value of pitch of the wrist 
           \param *yaw pointer to return value of yaw of the wrist
         */
     public: PR2_ERROR_CODE GetWristPoseGroundTruth(PR2_MODEL_ID id, double *x, double *y, double *z, double *roll, double *pitch, double *yaw);


         /*! \fn
           \brief Get the actual wrist pose 
           \param id - model ID, see pr2Core.h for a list of model IDs
         */
     public: NEWMAT::Matrix GetWristPoseGroundTruth(PR2_MODEL_ID id);

         /*! \fn
           \brief Get the actual joint values for an arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param jointPosition - array of desired positions of all the joints
           \param jointSpeed - array of desired speeds of all the joints
         */
      public: PR2_ERROR_CODE GetArmJointPositionActual(PR2_MODEL_ID id, double jointPosition[], double jointSpeed[]);


         /*! \fn
           \brief Command a desired torque for all the joints in an arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param torque - array of desired torques for all the joints
         */
      public: PR2_ERROR_CODE SetArmJointTorque(PR2_MODEL_ID id, double torque[]);

         /*! \fn
           \brief Get the commanded torque for all the joints in an arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param torque - array of desired torques for all the joints
         */
      public: PR2_ERROR_CODE GetArmJointTorqueCmd(PR2_MODEL_ID id, double torque[]);


         /*! \fn
           \brief Get the actual torque for all the joints in an arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param torque - array of actual torques for all the joints
         */
      public: PR2_ERROR_CODE GetArmJointTorqueActual(PR2_MODEL_ID id, double torque[]);



         /*! \fn
           \brief Command the arm to a particular speed in joint space
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param speed - array of desired speeds for all the joints
         */
      public: PR2_ERROR_CODE SetArmJointSpeed(PR2_MODEL_ID id, double speed[]);


         /*! \fn
           \brief Get the commanded joint speeds for the entire arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param speed - array of desired speeds for all the joints
         */
      public: PR2_ERROR_CODE GetArmJointSpeedCmd(PR2_MODEL_ID id, double speed[]);

         /*! \fn
           \brief Get the actual joint speeds for the entire arm
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param speed - array of desired speeds for all the joints
         */
      public: PR2_ERROR_CODE GetArmJointSpeedActual(PR2_MODEL_ID id, double speed[]);


         /*! \fn
           \brief Command the end-effector to go to a particular position in cartesian space
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param effectorPosition - array of desired position of the end-effector
           \param effectorSpeed - array of desired speed of the end-effector
         */
      //public: PR2_ERROR_CODE SetArmCartesianPosition(PR2_MODEL_ID id, double effectorPosition[], double effectorSpeed[]);
      public: PR2_ERROR_CODE SetArmCartesianPosition(PR2_MODEL_ID id, NEWMAT::Matrix g);


#ifdef KDL_KINEMATICS			
// KDL version of SetArmCartesianPosition
			public: PR2_ERROR_CODE SetArmCartesianPosition(PR2_MODEL_ID id, KDL::Frame f);
#endif

         /*! \fn
           \brief Get the commanded position and speed for the end-effector
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param effectorPosition - array of desired position of the end-effector
           \param effectorSpeed - array of desired speed of the end-effector
         */
      public: PR2_ERROR_CODE GetArmCartesianPositionCmd(PR2_MODEL_ID id, double effectorPosition[], double effectorSpeed[]);

         /*! \fn
           \brief Get the actual cartesian position and speed for the end-effector
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param effectorPosition - array of desired position of the end-effector
           \param effectorSpeed - array of desired speed of the end-effector
         */
      public: PR2_ERROR_CODE GetArmCartesianPositionActual(PR2_MODEL_ID id, double effectorPosition[], double effectorSpeed[]);

         /*! \fn
           \brief Forward Kinematics - Compute cartesian position and speed for the end-effector
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param angles - joint angles
         */
     public: NEWMAT::Matrix ComputeArmForwardKinematics(PR2_MODEL_ID id, double angles[]);

         /*! \fn
           \brief Inverse Kinematics - Compute joint angles corresponding to end-effector position and orientation
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param g - representation of end-effector position and orientation
         */
     public: NEWMAT::Matrix ComputeArmInverseKinematics(PR2_MODEL_ID id, NEWMAT::Matrix g);

         /*! \fn
           \brief Command a desired end-effector force
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param force - desired end effector force
         */
      public: PR2_ERROR_CODE SetArmCartesianForce(PR2_MODEL_ID id, double force[]);

         /*! \fn
           \brief Get the commanded end-effector force
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param force - desired end effector force
         */
      public: PR2_ERROR_CODE GetArmCartesianForceCmd(PR2_MODEL_ID id, double force[]);

         /*! \fn
           \brief Get the actual end-effector force
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param force - desired end effector force
         */
      public: PR2_ERROR_CODE GetArmCartesianForceActual(PR2_MODEL_ID id, double force[]);

         /*! \fn
           \brief Command a desired speed in cartesian space
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param speed - desired end effector speed
         */
      public: PR2_ERROR_CODE SetArmCartesianSpeed(PR2_MODEL_ID id, double speed[]);

         /*! \fn
           \brief Get the commanded speed in cartesian space
           \param id - model ID, see pr2Core.h  for a list of model IDs
           \param force - desired end effector speed
         */
      public: PR2_ERROR_CODE GetArmCartesianSpeedCmd(PR2_MODEL_ID id, double speed[]);


         /*! \fn
           \brief Get the actual end-effector speed in cartesian space
           \param id - model ID, see pr2Core.h for a list of model IDs
           \param force - desired end effector speed
         */
      public: PR2_ERROR_CODE GetArmCartesianSpeedActual(PR2_MODEL_ID id, double speed[]);

         /*! \fn
           \brief Set a control mode for the base
           \param mode - two choices - Cartesian or Joint control
         */
      public: PR2_ERROR_CODE SetBaseControlMode(PR2_CONTROL_MODE mode);


         /*! \fn
           \brief Get a control mode for the base
           \param *mode - pointer to return value - two choices - Cartesian or Joint control
         */
      public: PR2_ERROR_CODE GetBaseControlMode(PR2_CONTROL_MODE *mode);


         /*! \fn
           \brief Command a desired speed for the base in cartesian space in body coordinates 
           \param vx - forward speed
           \param vy - sideways speed
           \param vw - rotational speed
         */
      public: PR2_ERROR_CODE SetBaseCartesianSpeedCmd(double vx, double vy, double vw);

         /*! \fn
           \brief Command a steering angle to achieve the desired speed. At the end of this maneuver the wheels will be at the steering angles required to achieve the desired combination of translational and rotational speeds.
           \param vx - forward speed
           \param vy - sideways speed
           \param vw - rotational speed
         */
      public: PR2_ERROR_CODE SetBaseSteeringAngle(double vx, double vy, double vw);

         /*! \fn
           \brief Retrieve commanded speed for the base in cartesian space in body coordinates 
           \param vx - forward speed
           \param vy - sideways speed
           \param vw - rotational speed
         */
      public: PR2_ERROR_CODE GetBaseCartesianSpeedCmd(double* vx, double* vy, double* vw);

         /*! \fn
           \brief Retrieve caster/wheel properties and estimate velocity of the base in cartesian space in body coordinates 
           \param vx - forward speed
           \param vy - sideways speed
           \param vw - rotational speed
         */
      public: PR2_ERROR_CODE GetBaseCartesianSpeedActual(double* vx, double* vy, double* vw);

         /*! \fn
           \brief Retrieve base box position
           \param x - forward
           \param y - sideways (left)
           \param w - upward
         */
      public: PR2_ERROR_CODE GetBasePositionActual(double* x, double* y, double* z);

         /*! \fn
           \brief - Run the robot
         */
      public: PR2_ERROR_CODE RunRobot();


         /*! \fn
           \brief - Stop the robot
         */
      public: PR2_ERROR_CODE StopRobot();

         /*! \fn
           \brief - Get laser range data
         */
      public:    PR2_ERROR_CODE GetLaserRanges(PR2_SENSOR_ID id,
          float* angle_min, float* angle_max, float* angle_increment,
          float* range_max,uint32_t* ranges_size     ,uint32_t* ranges_alloc_size,
                           uint32_t* intensities_size,uint32_t* intensities_alloc_size,
                           float*    ranges          ,uint8_t*  intensities);

         /*! \fn
           \brief - Open gripper
         */
      public: PR2_ERROR_CODE OpenGripper(PR2_MODEL_ID id,double gap, double force);

         /*! \fn
           \brief - Close gripper
         */
      public: PR2_ERROR_CODE CloseGripper(PR2_MODEL_ID id,double gap, double force);

         /*! \fn
           \brief - Set gripper p,i,d gains
         */
      public: PR2_ERROR_CODE SetGripperGains(PR2_MODEL_ID id,double p,double i, double d);
         /*! 
           \brief - control mode for the arms, possible values are joint space control or cartesian space control
         */

         /*! \fn
           \brief - Get camera data
         */
      public:    PR2_ERROR_CODE GetCameraImage(PR2_SENSOR_ID id ,
                     uint32_t*    width                 ,uint32_t*    height                ,
                     uint32_t*    depth                 ,
                     std::string* compression           ,std::string* colorspace            ,
                     uint32_t*    data_size             ,void*        data                  );

      protected: PR2_CONTROL_MODE armControlMode[2];
      protected: PR2_CONTROL_MODE baseControlMode;
      protected: PR2Arm myArm; 

#ifdef KDL_KINEMATICS
      public: PR2_kinematics pr2_kin; // for kinematics using KDL.
#endif
         /*! \fn
           \brief - Oscillate the Hokuyo, return point cloud
         */
      //public: PR2_ERROR_CODE GetPointCloud();

         /*! \fn
           \brief - returns simulation time
         */
      public:    PR2_ERROR_CODE GetSimTime(double *sim_time);



    };
}
#endif
