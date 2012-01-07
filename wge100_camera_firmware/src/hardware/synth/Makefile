project = wge100
vendor = xilinx
family = spartan3e
part = xc3s500e-4cp132 # Camera
#part = xc3s500e-5fg320 # 3E Starter
#part = xc3s700a-5fg484 # 3A Starter
#top_module = top
top_module = topj1
constraints = wge100_RevC_Camera.ucf
#top_module = dram

libdir = ../lib
libs=mac

xilinx_cores += ../verilog/coregen/pixfifo.xco

vfiles =\
  ../verilog/topj1.v \
  ../verilog/watchdog.v \
  ../verilog/j1.v \
  ../verilog/uart.v \
  ../verilog/ck_div.v \
  ../verilog/reset_gen.v \
  ../verilog/trig_watchdog.v \
#

include $(libdir)/synth/xilinx.mk

wge100.ucf: $(constraints)
	cp $(constraints) wge100.ucf
junk += wge100.ucf
