#! /bin/sh 

ps aux|grep -v grep | grep wge100_camera_node
if [ $? = 0 ] ; then
  echo
  echo Detected that the wge100 driver is running.
  echo Please kill it and try again.
  exit 1
fi

if test ! -e default.mcs; then
  echo This script must be run from the wge100_camera/firmware_images
  echo directory.
  exit 1
fi

echo '********************************************************'
echo WARNING: This script will reflash all the wge100 cameras
echo on this robot. You must not interrupt this operation as
echo the cameras could end up in an undefined state from which
echo they will be unable to boot.
echo 
echo Before running this script, make sure that none of the
echo wge100 cameras are currently running, and do not launch
echo the camera driver while the script is running. Also, do
echo not interrupt power to the robot or reset the cameras 
echo while the script is running.
echo '********************************************************'
echo
echo To proceed, press CTRL+D. To abort press CTRL+C.
cat

set -e
for s in name://wide_stereo_l name://wide_stereo_r name://narrow_stereo_l name://narrow_stereo_r name://forearm_r name://forearm_l; do
  rosrun wge100_camera upload_mcs default.mcs $s#lan0
done
