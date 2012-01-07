import subprocess
import re

disc = subprocess.Popen(["rosrun", "wge100_camera", "discover", "lan1"], stdout=subprocess.PIPE).stdout
lines = list(disc)
cc = re.compile(r'^Found.*serial://(?P<serial>[0-9]*) name://(?P<name>[^ ]*) .* IP: (?P<ip>[0-9.]*),')
a0 = [cc.match(l) for l in lines]
cams = [m.groupdict() for m in a0 if m]
for i,cam in enumerate(cams):
    print "rosrun wge100_camera set_name serial://%s@10.69.0.%d %s %s" % (cam['serial'], 100 + i, cam['name'], cam['ip'].replace('68', '69'))
