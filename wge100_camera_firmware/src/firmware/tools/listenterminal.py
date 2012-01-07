import serial
import time
import sys

speed = 115200

if 0:
    ser = serial.Serial('/dev/ttyUSB0', speed, timeout=None, rtscts=1)
    ser.sendBreak()
    ser.close()
    time.sleep(0.1)

log = open("log", "w")
print ">>>"
ser = serial.Serial('/dev/ttyUSB0', speed, timeout=None, rtscts=1)
s = None
while s != '#':
    s = ser.read(1)
    sys.stdout.write(s)
    log.write(s)
