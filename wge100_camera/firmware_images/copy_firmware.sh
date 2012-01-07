#! /bin/sh -e
source=$1
dest=$2

echo Copying from $source'/wge100_ip_camera.(bit|mcs) to wge100_camera_0x'$2'.(bit|mcs)'
echo Press CTRL+D to start or CTRL+C to abort
cat

cp -v "$source/wge100_ip_camera.bit" "wge100_camera_0x$2.bit"
cp -v "$source/wge100_ip_camera.mcs" "wge100_camera_0x$2.mcs"

svn add "wge100_camera_0x$2.bit" "wge100_camera_0x$2.mcs"

svn rm --force default.mcs default.bit

svn cp "wge100_camera_0x$2.bit" default.bit
svn cp "wge100_camera_0x$2.mcs" default.mcs
