import sys
import random
import array
import time

from camcontroller import CamController


binfile = open(sys.argv[1]).read()
for target in sys.argv[2:]:
    print "Loading", target
    cc = CamController(target)
    for page in range(0x3e):
        e_addr = (2001 + page) * 512
        e_text = array.array('B', [random.randrange(256) for i in range(264)]).tostring()
        e_text = binfile[256 * page : 256 * (page + 1)] + (8 * chr(0))
        if cc.flashread(e_addr) != (e_addr, e_text):
            print "page %02x" % page
            cc.flashwrite(e_addr, e_text)
            (a_addr, a_text) = cc.flashread(e_addr)
            if (e_addr, e_text) != (a_addr, a_text):
                print "expected", "addr:", e_addr, "text", e_text
                print "actual  ", "addr:", a_addr, "text", a_text
                print "Try again", cc.flashread(e_addr)
                assert 0
    # cc.flashwrite(2000 * 512, struct.pack("!H262s", 947, ""))
    cc.flashlaunch(0)
