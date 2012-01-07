#! /bin/bash

i = 0
while true; do
  let i=i+1
  echo Starting cycle $i
  rostopic echo /camera/image_raw | head > /dev/null
  rosnode kill wge100_driver_cycle_test 
  sleep 1
done
