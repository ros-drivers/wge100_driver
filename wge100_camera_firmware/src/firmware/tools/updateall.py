import subprocess
import sys

# cams = subprocess.Popen("rosrun wge100_camera discover", stdout=subprocess.PIPE).stdout
cams = subprocess.Popen(["rosrun", "wge100_camera", "discover", "lan1"], stdout=subprocess.PIPE).stdout
serials = dict([l.split()[2:4] for l in cams if l.startswith('Found')])
for i,serial in enumerate(serials):
    print "rosrun wge100_camera upload_mcs %s %s@10.68.0.%d" % (sys.argv[1], serial, 100 + i)
