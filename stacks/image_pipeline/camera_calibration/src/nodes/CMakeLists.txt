rospack_add_executable(calibration_server calibration_server.cpp)
target_link_libraries(calibration_server camera_calibration)

#rospack_add_executable(calibration_client calibration_client.cpp)
#target_link_libraries(calibration_client camera_calibration)
