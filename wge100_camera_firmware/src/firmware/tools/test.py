import sys
import random
import array
import time

from camcontroller import CamController

def r(n): return random.randrange(n)

if __name__ == '__main__':
    cc = CamController(sys.argv[1])
    started = time.time()
    def uptime():
        (tu, rate) = cc.timer()
        return tu / float(rate)
    t0 = uptime()
    count = 0
    while True:
        # print (time.time() - started), (time.time() - started) - (uptime() - t0)
        def sensorread():
            a = r(256)
            (aa, av) = cc.sensorread(a)
            assert (aa == a)
        def flashread():
            a = r(4096) * 512
            (aa, ad) = cc.flashread(a)
            assert (aa == a)
            assert len(ad) == 264
        random.choice([uptime, flashread, sensorread])()
        count += 1
        if (count % 1000) == 0:
            print count, "operations"
