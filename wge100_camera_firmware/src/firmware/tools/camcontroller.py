import socket
import struct

"""
( 0 )    | handle-discover
( 1 )    | handle-configure
( 2 )    | handle-vidstart
( 3 )    | handle-vidstop
( 4 )    | handle-configureFPGA \ handle-reset
( 5 )    | handle-timer
( 6 )    | handle-flashread
( 7 )    | handle-flashwrite
( 8 )    | handle-trigcontrol
( 9 )    | handle-sensorread
( 10)    | handle-sensorwrite
( 11)    | handle-sensorselect
( 12)    | handle-imagermode
( 13)    | handle-imagersetres
( 14)    | handle-sysconfig
( 15)    | handle-configureFPGA
"""

class CamController:
    def __init__(self, host):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.25)
        self.hp = (host, 1627)
    def command(self, cmd, hrt, payload):
        data = struct.pack("!LL16s16s", 0xdeaf42, cmd, hrt, "") + payload
        while True:
            try:
                self.sock.sendto(data, self.hp)
                response = self.sock.recv(1024)
                break
            except socket.timeout:
                print "timeout"
                pass
        headerf = "!LL16s16s"
        headersz = struct.calcsize(headerf)
        response_h = response[:headersz]
        response_b = response[headersz:]
        # print struct.unpack(headerf, response_h)
        # print len(response_b), repr(response_b)
        return response_b

    def fcommand(self, cmd, txfmt, txd, rxfmt):
        hrts = {
            9: "SENSORREAD"
        }
        while True:
            response = self.command(cmd, hrts.get(cmd, ""), struct.pack(txfmt, *txd))
            if struct.calcsize(rxfmt) == len(response):
                break
        return struct.unpack(rxfmt, response)

    def recv(self):
        return self.sock.recv(1024)

    def vidstart     (self, *args): return self.fcommand( 2, "8BLH", 8 * (0,) + (0, 0), "!LL")
    def vidstop      (self, *args): return self.fcommand( 3, "", args, "!LL")
    def timer        (self, *args): return self.fcommand( 5, "",  args, "!QL")
    def flashread    (self, *args): return self.fcommand( 6, "!L", args, "!L264s")
    def flashwrite   (self, *args): return self.fcommand( 7, "!L264s", args, "!LL")
    def trigcontrol  (self, *args): return self.fcommand( 8, "!L",  args, "!LL")
    def sensorread   (self, *args): return self.fcommand( 9, "!B",  args, "!BH")
    def sensorwrite  (self, *args): return self.fcommand(10, "!BH", args, "!LL")
    def sensorselect (self, *args): return self.fcommand(11, "!BL", args, "!LL")
    def setres       (self, *args): return self.fcommand(13, "!HH", args, "!LL")
    def configureFPGA(self, *args): return self.fcommand(15, "",  args, "!LL")
    def flashlaunch  (self, *args): return self.fcommand(16, "!H",  args, "!LL")

