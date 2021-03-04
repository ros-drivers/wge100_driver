#! /usr/bin/env python
#*
#*  Copyright (c) 2009, Willow Garage, Inc.
#*  All rights reserved.
#*
#*  Redistribution and use in source and binary forms, with or without
#*  modification, are permitted provided that the following conditions
#*  are met:
#*
#*   * Redistributions of source code must retain the above copyright
#*     notice, this list of conditions and the following disclaimer.
#*   * Redistributions in binary form must reproduce the above
#*     copyright notice, this list of conditions and the following
#*     disclaimer in the documentation and/or other materials provided
#*     with the distribution.
#*   * Neither the name of the Willow Garage nor the names of its
#*     contributors may be used to endorse or promote products derived
#*     from this software without specific prior written permission.
#*
#*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
#*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#*  POSSIBILITY OF SUCH DAMAGE.
#***********************************************************

# Author: Blaise Gassend

# WGE100 camera configuration, non camera-specific settings.

PACKAGE='wge100_camera'

from driver_base.msg import SensorLevels
from dynamic_reconfigure.parameter_generator_catkin import *

def add_others(gen):
        enum_register_set = gen.enum([
            gen.const("PrimaryRegisterSet",   int_t, 0, "The primary register set is always used."),
            gen.const("AlternateRegisterSet", int_t, 1, "The alternate register set is always used."),
            gen.const("Auto",                 int_t, 2, "The trigger signal selects which register set to use.")],
            "Some wge100 cameras have two register sets. You can chose either set, or have the trigger signal select one of the sets. The camera images will go to camera or camera_alternate depending on which register set is active.")

	#       Name                         Type      Reconfiguration level             Description                                                                                                                                                           Default    Min   Max
	#gen.add("exit_on_fault",             bool_t,   SensorLevels.RECONFIGURE_RUNNING, "Indicates if the driver should exit when an error occurs.",                                                                                                          False) 
        gen.add("frame_id",                  str_t,    SensorLevels.RECONFIGURE_RUNNING, "Sets the TF frame from which the camera is publishing.",                                                                                                             "")
        gen.add("register_set",              int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Select the register set to work with. In auto mode, an extra pulse on the trigger signal indicates that the alternate set should be used.",                          0, 0, 3, enum_register_set)
	
        gen.add("width",                     int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Number of pixels horizontally.",                                                                                                                                     640,        1,   752)
        gen.add("height",                    int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Number of pixels vertically.",                                                                                                                                       480,        1,   480)
        gen.add("horizontal_binning",        int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Number of pixels to bin together horizontally.",                                                                                                                     1,          1,   4)
        gen.add("vertical_binning",          int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Number of pixels to bin together vertically.",                                                                                                                       1,          1,   4)
        gen.add("x_offset",         int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Horizontal offset between the center of the image and the center of the imager in pixels.",                                                                          0,         0, 752)
        gen.add("y_offset",           int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Vertical offset between the center of the image and the center of the imager in pixels.",                                                                            0,         0, 480)
        gen.add("mirror_x",                  bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Mirrors the image left to right.",                                                                                                                                   False)
        gen.add("mirror_y",                  bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Mirrors the image top to bottom.",                                                                                                                                   False)
        gen.add("rotate_180",                bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Rotates the image 180 degrees. Acts in addition to the mirorr parameters",                                                                                           False)
	
        gen.add("imager_rate",               double_t, SensorLevels.RECONFIGURE_CLOSE,   "Sets the frame rate of the imager. In externally triggered mode this must be more than trig_rate",                                                                   30,         1,   100)
        gen.add("ext_trig",                  bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Set camera to trigger from the external trigger input.",                                                                                                             False)
        gen.add("rising_edge_trig",          bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Indicates that the camera should trigger on rising edges (as opposed to falling edges).",                                                                            True)
        gen.add("trig_timestamp_topic",      str_t,    SensorLevels.RECONFIGURE_CLOSE,   "Sets the topic from which an externally trigged camera receives its trigger timestamps.",                               "")
        gen.add("trig_rate",                 double_t, SensorLevels.RECONFIGURE_CLOSE,   "Sets the expected triggering rate in externally triggered mode.",                                                                                                    30,         1,   100)
        gen.add("first_packet_offset",       double_t, SensorLevels.RECONFIGURE_RUNNING, "Offset between the end of exposure and the minimal arrival time for the first frame packet. Only used when using internal triggering.",                              0.0025,     0,   .02)
	
        gen.add("brightness",                int_t,    SensorLevels.RECONFIGURE_CLOSE,   "The camera brightness for automatic gain/exposure.",                                                                                                                 58,         1,   64)
        gen.add("auto_black_level",          bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Enables the automatic black-level detection.",                                                                                                                       False)
        gen.add("black_level_filter_length", int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Base 2 logarithm of the number of frames the black-level algorithm should average over.",                                                                            4,         0,   7)
        gen.add("black_level_step_size",     int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Maximum per-frame change in the auto black-level.",                                                                                                                  2,         0,   31)
        gen.add("black_level",               int_t,    SensorLevels.RECONFIGURE_CLOSE,   "Sets the black level.",                                                                                                                                              0,         -127,   127)
        gen.add("max_exposure",              double_t, SensorLevels.RECONFIGURE_CLOSE,   "Maximum exposure time in seconds in automatic exposure mode. Zero for automatic.",                                                                                   0,          0,   .1)
	
        gen.add("auto_exposure",             bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Sets the camera exposure duration to automatic. Causes the exposure setting to be ignored.",                                                                         True)
        gen.add("exposure",                  double_t, SensorLevels.RECONFIGURE_CLOSE,   "Maximum camera exposure time in seconds. The valid range depends on the video mode.",                                                                                0.01,       0,   0.1)
        gen.add("auto_gain",                 bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Sets the analog gain to automatic. Causes the gain setting to be ignored.",                                                                                          True)
        gen.add("gain",                      int_t,    SensorLevels.RECONFIGURE_CLOSE,   "The camera analog gain.",                                                                                                                                            32,         16,  127)
        gen.add("companding",                bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Turns on companding (a non-linear intensity scaling to improve sensitivity in dark areas).",                                                                         True)
	
        gen.add("auto_exposure_alternate",   bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Sets the alternate camera exposure duration to automatic. Causes the exposure_alternate setting to be ignored.",                                                     True)
        gen.add("exposure_alternate",        double_t, SensorLevels.RECONFIGURE_CLOSE,   "Alternate camera exposure in seconds. The valid range depends on the video mode.",                                                                                   0.01,       0,    .1)
        gen.add("auto_gain_alternate",       bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Sets the alternate analog gain to automatic. Causes the gain_alternate setting to be ignored.",                                                                      True)
        gen.add("gain_alternate",            int_t,    SensorLevels.RECONFIGURE_CLOSE,   "The alternate camera analog gain.",                                                                                                                                  32,         16,   127)
        gen.add("companding_alternate",      bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Turns on companding for the alternate imager register set.",                                                                                                          True)
        gen.add("test_pattern",              bool_t,   SensorLevels.RECONFIGURE_CLOSE,   "Turns on the camera's test pattern.",                                                                                                          False)
        gen.add("packet_debug",              bool_t,   SensorLevels.RECONFIGURE_RUNNING, "Enable debug mode for dropped packets in diagnostics",                                                                                     False)
