# kcf_tracker_ros
This package performs tracking using Kernelized Correlation Filters and communicates via ROS. Tested on the Typhoon H480 drone simulation.

# Installation
Tested on Ubuntu 20.04 - ROS-noetic - OpenCV 4.2.0

* Install PX4 simulation via link: https://docs.px4.io/main/en/
* Copy the modified `typhoon_h480` model to `PX4-Autopilot/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models`
    ```sh
    cp -r ~/your_ws/src/kcf_tracker_ros/models/typhoon_h480 ~/PX4-Autopilot/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models
    ```
* Build
    ```sh
    cd your_ws/
    catkin build
    ```
# Demo


https://github.com/user-attachments/assets/c698592f-cf39-4004-8b7c-210c6e7b9604


# Reference
* [PX4-Autopilot](https://github.com/PX4/PX4-Autopilot)
* [mavros_apriltag_tracking](https://github.com/mzahana/mavros_apriltag_tracking)
* [Prometheus](https://github.com/amov-lab/Prometheus)
