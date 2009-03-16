#include "ros/node.h"
#include "image_msgs/Image.h"
#include "image_msgs/CamInfo.h"
#include "image_msgs/FillImage.h"
#include "prosilica_cam/PolledImage.h"

#include <cv.h>
#include <cvwimage.h>
#include <highgui.h>

#include <cstdio>
#include <cassert>

static const char INTRINSICS_FILE[] = "intrinsics.yml";

class FakePublisher
{
private:
  std::vector<std::string> files_;
  std::vector<std::string>::const_iterator current_iter_, end_iter_;
  CvMat *K_, *D_;

public:
  FakePublisher(const std::vector<std::string> &files)
    : files_(files), current_iter_(files_.begin()), end_iter_(files_.end()),
      K_(NULL), D_(NULL)
  {
    // Read camera matrix and distortion from YML file
    CvFileStorage* fs = cvOpenFileStorage(INTRINSICS_FILE, 0, CV_STORAGE_READ);
    assert(fs);
    K_ = (CvMat*)cvReadByName(fs, 0, "camera_matrix");
    D_ = (CvMat*)cvReadByName(fs, 0, "distortion_coefficients");
    cvReleaseFileStorage(&fs);

    ros::Node::instance()->advertiseService("/prosilica/poll", &FakePublisher::grab, this);
  }

  ~FakePublisher()
  {
    cvReleaseMat(&K_);
    cvReleaseMat(&D_);
  }

  bool grab(prosilica_cam::PolledImage::Request &req,
            prosilica_cam::PolledImage::Response &res)
  {
    if (current_iter_ == end_iter_) {
      ros::Node::instance()->unadvertiseService("/prosilica/poll");
      return false;
    }
    
    // Load image from file
    ROS_FATAL("Loading %s", current_iter_->c_str());
    cv::WImageBuffer3_b img( cvLoadImage(current_iter_->c_str()) );
    if (img.IsNull())
      return false;

    // Copy into result
    fillImage(res.image, "image", img.Height(), img.Width(), 3,
              "bgr", "uint8", img.ImageData());
    
    // Copy cam info we care about
    memcpy(&res.cam_info.D[0], D_->data.db, 5*sizeof(double));
    memcpy(&res.cam_info.K[0], K_->data.db, 9*sizeof(double));

    current_iter_++;

    return true;
  }
};

int main(int argc, char** argv)
{
  std::vector<std::string> files;
  files.reserve(argc - 1);
  for (int i = 1; i < argc; ++i) {
    //printf("File name: %s\n", argv[i]);
    files.push_back( argv[i] );
  }
  
  ros::init(argc, argv);
  ros::Node n("fake_publisher");
  FakePublisher fp(files);
  
  n.spin();

  return 0;
}
