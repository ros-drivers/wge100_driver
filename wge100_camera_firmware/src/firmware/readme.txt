How to build
============

This directory contains the source code for the WGE camera,
and the tools required to build it from scratch.  System
prerequisites: gforth

  sudo apt-get install gforth

The tools use a cross-compiler (crossj1.fs) to create a
Forth-subset for the target machine.  This is a subset of
ANS Forth:

  http://forth.sourceforge.net/standard/dpans/
  http://forth.sourceforge.net/standard/dpans/dpansf.htm

If you've never used Forth before, or need a refresher:

  http://www.forth.com/starting-forth/
  http://thinking-forth.sourceforge.net/

Running 'make j1.mem' builds a 16Kbyte flash image, and writes the
following files:

lst     readable listing of generated flash image
j1.mem  flash image suitable for Xilinx data2mem tool (see "./send" below)
j1.bin  flash binary image (see bootloader.py below)

Other build targets:
wge100_ip_camera.bit used when loading the FPGA via JTAG
wge100_ip_camera.mcs used when loading the flash FPGA image

Running on the camera
=====================

To load the code into a running camera using JTAG (requires Xilinx tools installed)::

  make wge100_ip_camera.bit && ./send wge100_ip_camera.bit

To load the code permanently into a camera (requires Xilinx tools installed)::

  make wge100_ip_camera.mcs && rosrun wge100_camera upload_mcs wge100_ip_camera.mcs serial://777

Often you'll want to quickly try out your code.  This command loads the
code into the camera and runs it once.  On the next reboot the camera
will revert to the old, permanent code.  Hence this method is safest::

  make j1.bin && python tools/bootloader.py 10.0.5.87 j1.bin

How to read the source
======================

Literals in the cross code
--------------------------

One aspect of the target code is that it uses words 'd#' and
'h#' for decimal and hex numbers::

  d# 12         \ pushes 12
  h# 30         \ pushes 48
  h# deadbeef.  \ pushes 0xbeef and 0xdead

The prefixes are mandatory in target code bcause d# and h#
mean "parse a number and emit the target code to push it".
This is a technique copied from eforth:
http://www.baymoon.com/~bimu/forth/

Source conventions
------------------

As per Brodie: 64-columns, stack diagrams for anything
non-trivial.  Split all functional areas into their own file.

Tags and editors
----------------

With highly factored code it is convenient to be able to
move to definitions easily.  The code builds a file "tags"
which vim and emacs can use.  For vim, the following settings
allow an unsorted tags file::

  set notagbsearch

Also for vim users, these settings give vim a more Forth-like
idea of what a word is::

  au BufNewFile,BufReadPost *.fs setlocal filetype=forth
  au BufNewFile,BufReadPost *.fs setlocal iskeyword=!,@,33-35,%,$,38-64,A-Z,91-96,a-z,123-126,128-255
  au BufNewFile,BufReadPost *.fs setlocal sw=4 ts=4

About the camera
================

External trigger
----------------

The drivers sets the frame timing registers so that a
complete frame and expose can happen faster than the pulse
on EXT_TRIG_B. Then every time exposing_i goes low, the
WGE stops the imager's clock.  When EXT_TRIG_B goes active,
WGE starts the imager's clock again.
