#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <chrono>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <ros/ros.h>
#include <ros/package.h>
#include <yaml-cpp/yaml.h>
#include <image_transport/image_transport.h>  
#include <cv_bridge/cv_bridge.h>  
#include <sensor_msgs/image_encodings.h>
#include <geometry_msgs/Pose.h>
#include <std_msgs/Bool.h>
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/ml.hpp>
#include <kcf_tracker_ros/DetectionInfo.h>

#include "kcftracker.hpp"


using namespace std;
using namespace cv;


static const std::string RGB_WINDOW = "RGB Image window";

int frame_width, frame_height;
std_msgs::Header image_header;
cv::Mat cam_image_copy;
boost::shared_mutex mutex_image_callback;
bool image_status = false;
boost::shared_mutex mutex_image_status;

ros::Subscriber switch_subscriber;

bool is_suspanded = false;

bool local_print = false;


void cameraCallback(const sensor_msgs::ImageConstPtr& msg)
{
    if (local_print)
        ROS_DEBUG("[KCFTracker] USB image received.");

    cv_bridge::CvImagePtr cam_image;

    try {
        cam_image = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
        image_header = msg->header;
    } catch (cv_bridge::Exception& e) {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }

    if (cam_image) {
        {
            boost::unique_lock<boost::shared_mutex> lockImageCallback(mutex_image_callback);
            cam_image_copy = cam_image->image.clone();
        }
        {
            boost::unique_lock<boost::shared_mutex> lockImageStatus(mutex_image_status);
            image_status = true;
        }
        frame_width = cam_image->image.size().width;
        frame_height = cam_image->image.size().height;
    }
    return;
}

bool getImageStatus(void)
{
    boost::shared_lock<boost::shared_mutex> lock(mutex_image_status);
    return image_status;
}

image_transport::Subscriber imageSubscriber_;
ros::Publisher pose_pub;
kcf_tracker_ros::DetectionInfo pose_now;

cv::Rect selectRect;
cv::Point origin;
cv::Rect result;

bool select_flag = false;
bool bRenewROI = false; 
bool bBeginKCF = false;

void onMouse(int event, int x, int y, int, void*)
{
    if (select_flag)
    {
        selectRect.x = MIN(origin.x, x);        
        selectRect.y = MIN(origin.y, y);
        selectRect.width = abs(x - origin.x);   
        selectRect.height = abs(y - origin.y);
        selectRect &= cv::Rect(0, 0, frame_width, frame_height);
    }
    if (event == cv::EVENT_LBUTTONDOWN)
    {
        bBeginKCF = false;  
        select_flag = true; 
        origin = cv::Point(x, y);       
        selectRect = cv::Rect(x, y, 0, 0);  
    }
    else if (event == cv::EVENT_LBUTTONUP)
    {
        if (selectRect.width*selectRect.height < 64)
        {
            ;
        }
        else
        {
            select_flag = false;
            bRenewROI = true;    
        } 
    }
}


void switchCallback(const std_msgs::Bool::ConstPtr& msg)
{
    is_suspanded = !(bool)msg->data;
}


bool HOG = true;
bool FIXEDWINDOW = false;
bool MULTISCALE = true;
bool SILENT = true;
bool LAB = false;

// Create KCFTracker object
KCFTracker tracker(HOG, FIXEDWINDOW, MULTISCALE, LAB);


int main(int argc, char **argv)
{
    ros::init(argc, argv, "kcf_tracker");
    ros::NodeHandle nh("~");
    image_transport::ImageTransport it(nh); 
    ros::Rate loop_rate(30);

    std::string camera_topic, camera_info;
    if (nh.getParam("camera_topic", camera_topic)) {
        ROS_INFO("camera_topic is %s", camera_topic.c_str());
    } else {
        ROS_WARN("didn't find parameter camera_topic");
        camera_topic = "/cgo3_camera/image_raw";
    }

    if (nh.getParam("camera_info", camera_info)) {
        ROS_INFO("camera_info is %s", camera_info.c_str());
    } else {
        ROS_WARN("didn't find parameter camera_info");
        camera_info = "camera_param.yaml";
    }

    bool switch_state = is_suspanded;
    switch_subscriber = nh.subscribe("/switch/kcf_tracker", 10, switchCallback);

    imageSubscriber_ = it.subscribe(camera_topic.c_str(), 1, cameraCallback);

    pose_pub = nh.advertise<kcf_tracker_ros::DetectionInfo>("/object_detection/kcf_tracker", 1);
    
    sensor_msgs::ImagePtr msg_ellipse;

    std::string ros_path = ros::package::getPath("kcf_tracker_ros");
    cout << "DETECTION_PATH: " << ros_path << endl;

    YAML::Node camera_config = YAML::LoadFile(camera_info);
    double fx = camera_config["fx"].as<double>();
    double fy = camera_config["fy"].as<double>();
    double cx = camera_config["x0"].as<double>();
    double cy = camera_config["y0"].as<double>();
    double k1 = camera_config["k1"].as<double>();
    double k2 = camera_config["k2"].as<double>();
    double p1 = camera_config["p1"].as<double>();
    double p2 = camera_config["p2"].as<double>();
    double k3 = camera_config["k3"].as<double>();

    double kcf_tracker_h = camera_config["kcf_tracker_h"].as<double>();

    const auto wait_duration = std::chrono::milliseconds(2000);

    cv::namedWindow(RGB_WINDOW);
    cv::setMouseCallback(RGB_WINDOW, onMouse, 0);
    float last_x(0), last_y(0), last_z(0), last_ax(0), last_ay(0);

    while (ros::ok())
    {
        while (!getImageStatus() && ros::ok()) 
        {
            cout << "Waiting for image." << endl;
            std::this_thread::sleep_for(wait_duration);
            ros::spinOnce();
        }

        if (switch_state != is_suspanded)
        {
            switch_state = is_suspanded;
            if (!is_suspanded)
            {
                cout << "Start Detection." << endl;
            }
            else
            {
                cout << "Stop Detection." << endl;
            }
        }

        Mat frame;
        {
            boost::unique_lock<boost::shared_mutex> lockImageCallback(mutex_image_callback);
            frame = cam_image_copy.clone();
        }
        if (!is_suspanded)
        {
        static bool need_tracking_det = false;

        bool detected = false;
        if (bRenewROI)
        {
            tracker.init(selectRect, frame);
            cv::rectangle(frame, selectRect, cv::Scalar(255, 0, 0), 2, 8, 0);
            bRenewROI = false;
            bBeginKCF = true;
        }
        else if (bBeginKCF)
        {
            result = tracker.update(frame);
            cv::rectangle(frame, result, cv::Scalar(255, 0, 0), 2, 8, 0);

            detected = true;
            pose_now.header.stamp = ros::Time::now();
            pose_now.detected = true;
            pose_now.frame = 0;
            double depth = kcf_tracker_h / result.height * fy;
            double cx = result.x + result.width / 2 - frame.cols / 2;
            double cy = result.y + result.height / 2 - frame.rows / 2;
            pose_now.position[0] = depth * cx / fx;
            pose_now.position[1] = depth * cy / fy;
            pose_now.position[2] = depth;

            pose_now.sight_angle[0] = cx / (frame.cols / 2) * atan((frame.cols / 2) / fx);
            pose_now.sight_angle[1] = cy / (frame.rows / 2) * atan((frame.rows / 2) / fy);

            last_x = pose_now.position[0];
            last_y = pose_now.position[1];
            last_z = pose_now.position[2];
            last_ax = pose_now.sight_angle[0];
            last_ay = pose_now.sight_angle[1];
        }
        else if(select_flag)
        {
            cv::rectangle(frame, selectRect, cv::Scalar(255, 0, 0), 2, 8, 0);
        }

        if (!detected)
        {
            pose_now.header.stamp = ros::Time::now();
            pose_now.detected = false;
            pose_now.frame = 0;
            pose_now.position[0] = last_x;
            pose_now.position[1] = last_y;
            pose_now.position[2] = last_z;
            pose_now.sight_angle[0] = last_ax;
            pose_now.sight_angle[1] = last_ay;
        }

        pose_pub.publish(pose_now);
        }
        else
        {
            selectRect = cv::Rect(0, 0, 0, 0);
            bRenewROI = false;
            bBeginKCF = false;
        }

        imshow(RGB_WINDOW, frame);

        waitKey(5);
        ros::spinOnce();
        loop_rate.sleep();
    }
}




