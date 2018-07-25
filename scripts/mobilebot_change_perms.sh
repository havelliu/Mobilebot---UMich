#!/bin/sh
# shell script for changing ownership and sticky bit for mobilebot_ros
# usage: ~/bin/mobilebot_change_perms.sh
#
echo ls -l ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros
ls -l ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros

echo sudo chown root:root  ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros
sudo chown root:root  ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros

echo sudo chmod u+s  ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros
sudo chmod u+s  ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros

echo ls -l ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros
ls -l ~/catkin_ws/devel/lib/mobilebot_ros/mobilebot_ros
