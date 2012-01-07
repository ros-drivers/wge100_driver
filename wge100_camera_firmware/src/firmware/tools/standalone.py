#!/usr/bin/python

import sys
import random
import array
import time
import struct

from camcontroller import CamController

ip = sys.argv[1]

i2cs = [
    (0x0018, 0x3E3A),
    (0x0015, 0x7F32),
    (0x0020, 0x01C1),
    (0x0021, 0x0018),
    (0x0070, 0x0014),
    (0x0006, 0x0034),
    (0x0005, 0x0233),
    (0x0001, 0x0039),
    (0x0002, 0x0004),
    (0x0004, 0x0280),
    (0x0003, 0x01E0),
    (0x000D, 0x0300),
    (0x001C, 0x0003),
    (0x00AF, 0x0003),
    (0x0035, 0x8020),
    (0x000D, 0x0300),
    (0x000B, 0x0085),
    (0x0047, 0x0081),
    (0x0048, 0x0000),
    (0x004C, 0x0002),
    (0x00A5, 0x003A),
    (0x00BD, 0x020E),
]

import cv

def main():
    im = cv.CreateMat(480, 640, cv.CV_8UC1)
    line1 = cv.CreateMat(1, 640, cv.CV_8UC1)
    rgbim = cv.CreateMat(480, 640, cv.CV_8UC3)
    font = cv.InitFont(cv.CV_FONT_HERSHEY_SIMPLEX, .9, 1, thickness = 2, lineType=cv.CV_AA)

    cc = CamController(ip)
    (num,denom) = cc.timer()
    uptime = num / float(denom)

    temp = 0

    cc.vidstop()
    cc.setres(640, 480)
    for (reg, val) in i2cs:
        cc.sensorwrite(reg, val)
    cc.trigcontrol(0)
    cc.sensorselect(0, 0xc1)    # 0xc1 temperature in slot 0
    cc.vidstart()
    while True:
        data = cc.recv()
        if len(data) == 34:
            (frame, lineFFFF, cols, rows, tnum, tdenom, sens0, sens1, sens2, sens3, sensok) =  struct.unpack("!LHHHQL4HL", data)
            uptime = tnum / float(tdenom)
            temp = sens0
        if len(data) == 650:
            (frame, line, cols, rows, pixels) = struct.unpack("!LHHH640s", data)
            if 0 <= line < 480:
                cv.SetData(line1, data)
                cv.Copy(line1, cv.GetSubRect(im, (0, line, 640, 1)))
                if line == 479:
                    cv.CvtColor(im, rgbim, cv.CV_BayerRG2BGR)
                    cv.PutText(rgbim, "frame: %4d  uptime: %5.0f temp: %d" % (frame, uptime, temp), (10, 400), font, (255,255,255))
                    cv.ShowImage("Camera", rgbim)
                    cv.WaitKey(6)

main()
