cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

rospack(move_arm)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

genmsg()

# main executable 
rospack_add_executable(move_arm_action src/move_arm_setup.cpp src/move_arm_action.cpp)
rospack_link_boost(move_arm_action thread)

# actuate gripper action
rospack_add_executable(actuate_gripper_action src/actuate_gripper_action.cpp)

# teleoperation
rospack_add_executable(teleop_arm src/teleop_arm.cpp src/move_arm_setup.cpp)


# debugging tools

rospack_add_executable(arm_cmd_line src/arm_cmd_line.cpp)
rospack_link_boost(arm_cmd_line thread)

rospack_add_executable(record_state src/record_state.cpp)
rospack_link_boost(record_state thread)

rospack_add_executable(record_path src/record_path.cpp)
rospack_link_boost(record_path thread)

# simple tests
rospack_add_executable(oscillate_move test/oscillate_move.cpp)
rospack_link_boost(oscillate_move thread)

# actuate gripper action test
rospack_add_executable(actuate_gripper test/actuate_gripper.cpp)
