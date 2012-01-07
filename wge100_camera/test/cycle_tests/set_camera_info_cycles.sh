#! /bin/bash

# Repeatedly call the set_camera_info service to make sure it never
# crashes.

if [ $1_foo = _foo ]; then
  echo Need to specify a camera namespace.
  exit 1
fi

let i=0
let err=0
while true; do
  let i=i+1
  echo Cycle "$i (errors $err)"
  rosservice call $1/set_camera_info \
"camera_info:
  header:
    seq: 1389
    stamp: 1262890038558022000
    frame_id:
  height: 480
  width: 640
  roi:
    x_offset: 0
    y_offset: 0
    height: 0
    width: 0
  D: [0.0, 0.0, 0.0, 0.0, 0.0]
  K: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
  R: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
  P: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]" || let err=err+1
done
