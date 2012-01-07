#! /bin/bash

# Repeatedly reconfigures a camera to make sure it always comes up alright.

let i=0
rosrun wge100_camera reconfigure_cam $1 2> /dev/null
while true; do
  let i=i+1
  echo -n Cycle "$i"
  let j=0
  while true; do 
    rosrun wge100_camera reconfigure_cam $1 2> /dev/null && break
    echo -n "."
    sleep 1
  done
  echo
done
