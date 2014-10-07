#include "dvs_calibration/mono_dvs_calibration.h"

MonoDvsCalibration::MonoDvsCalibration()
{
  gotCameraInfo = false;

  calibration_running = false;

  setCameraInfoClient = nh.serviceClient<sensor_msgs::SetCameraInfo>("set_camera_info");
  cameraInfoSubscriber = nh.subscribe("camera_info", 1, &MonoDvsCalibration::cameraInfoCallback, this);

  // add transition map
  const int camera_id = 1;
  transition_maps.insert(std::pair<int, TransitionMap>(camera_id, TransitionMap()));
  eventSubscriber = nh.subscribe<dvs_msgs::EventArray>("events", 1,
                                                       boost::bind(&MonoDvsCalibration::eventsCallback, this, _1, camera_id));


  cameraInfoPublisher = nh.advertise<sensor_msgs::CameraInfo>("dvs_calibration/camera_info", 1);
  reprojectionErrorPublisher = nh.advertise<std_msgs::Float64>("dvs_calibration/calibration_reprojection_error", 1);
  cameraPosePublisher = nh.advertise<geometry_msgs::PoseStamped>("dvs_calibration/pose", 1);

  image_transport::ImageTransport it(nh);
  patternPublisher = it.advertise("dvs_calibration/pattern", 1);
  visualizationPublisher = it.advertise("dvs_calibration/visualization", 1);
}

void MonoDvsCalibration::calibrate()
{
  cv::Mat cameraMatrix, distCoeffs;
  std::vector<cv::Mat> rvecs, tvecs;
  double reproj_error = cv::calibrateCamera(object_points, image_points, cv::Size(sensor_width, sensor_height),
                                            cameraMatrix, distCoeffs, rvecs, tvecs, CV_CALIB_FIX_K3);

  sensor_msgs::CameraInfo msg;

  msg.height = sensor_height;
  msg.width = sensor_width;
  msg.distortion_model = "plumb_bob";

  for (int i = 0; i < 5; i++)
    msg.D.push_back(distCoeffs.at<double>(i));
  for (int i = 0; i < 9; i++)
    msg.K[i] = cameraMatrix.at<double>(i);

  cameraInfoPublisher.publish(msg);
  cameraInfo = msg;

  std_msgs::Float64 msg2;
  msg2.data = reproj_error;
  reprojectionErrorPublisher.publish(msg2);
}

void MonoDvsCalibration::publishVisualizationImage(cv::Mat image)
{
  cv_bridge::CvImage cv_image;
  cv_image.encoding = "bgr8";
  cv_image.image = image.clone();

  visualizationPublisher.publish(cv_image.toImageMsg());
}
//
//void MonoDvsCalibration::publishPatternImage(cv::Mat image)
//{
//  cv_bridge::CvImage cv_image;
//  cv_image.encoding = "mono8";
//  cv_image.image = image.clone();
//  patternPublisher.publish(cv_image.toImageMsg());
//}

bool MonoDvsCalibration::setCameraInfo()
{
  sensor_msgs::SetCameraInfo srv;
  srv.request.camera_info = cameraInfo;
  return setCameraInfoClient.call(srv);
}

void MonoDvsCalibration::cameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr& msg)
{
  gotCameraInfo = true;
  cameraInfo = *msg;
}

void MonoDvsCalibration::resetCalibration()
{
  const int camera_id = 1;
  transition_maps[camera_id].reset_maps();
  calibration_running = false;
  object_points.clear();
  image_points.clear();
  num_detections = 0;
}

void MonoDvsCalibration::startCalibration()
{
  calibration_running = true;
  if (num_detections > 0)
  {
    calibrate();
  }
}

void MonoDvsCalibration::saveCalibration()
{
  setCameraInfo();
}

void MonoDvsCalibration::add_pattern(int id)
{
  // add detection
  image_points.push_back(transition_maps[id].pattern);
  object_points.push_back(world_pattern);
  num_detections++;

  // compute and publish camera pose if camera is calibrated
  // TODO
}

void MonoDvsCalibration::update_visualization(int id)
{
  publishVisualizationImage(transition_maps[id].get_visualization_image());
}