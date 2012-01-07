/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, 2010 Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#ifndef __FOREARM_CAM__MT9V_H__
#define __FOREARM_CAM__MT9V_H__

#include <stdlib.h>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <wge100_camera/wge100lib.h>

#define MT9V_REG_WINDOW_WIDTH 0x04
#define MT9V_REG_HORIZONTAL_BLANKING 0x05
#define MT9V_REG_VERTICAL_BLANKING 0x06
#define MT9V_REG_TOTAL_SHUTTER_WIDTH 0x0B
#define MT9V_REG_ANALOG_GAIN 0x35
#define MT9V_REG_AGC_AEC_ENABLE 0xAF
#define MT9V_COMPANDING_MODE 0x1C

#define MT9V_CK_FREQ 16e6

/*
 * Imager Modes
 */
#define MT9VMODE_752x480x15b1 		0
#define MT9VMODE_752x480x12_5b1 	1
#define MT9VMODE_640x480x30b1		2
#define MT9VMODE_640x480x25b1		3
#define MT9VMODE_640x480x15b1		4
#define MT9VMODE_640x480x12_5b1		5
#define MT9VMODE_320x240x60b2		6
#define MT9VMODE_320x240x50b2		7
#define MT9VMODE_320x240x30b2		8
#define MT9VMODE_320x240x25b2 		9
#define MT9V_NUM_MODES 10

struct MT9VMode {
  const char *name;
  size_t width;
  size_t height;
  double fps;
  uint16_t hblank;
  uint16_t vblank;
};

extern const struct MT9VMode MT9VModes[MT9V_NUM_MODES];

class MT9VImager;
typedef boost::shared_ptr<MT9VImager> MT9VImagerPtr;

// Methods return true on failure.
class MT9VImager 
{
public:
  virtual bool setBrightness(int brightness) = 0;
  virtual bool setAgcAec(bool agc_on, bool aec_on) = 0;
  virtual bool setGain(int gain) = 0;
  virtual bool setExposure(double exposure) = 0;
  virtual bool setMaxExposure(double exposure) = 0;
  virtual bool setMirror(bool mirrorx, bool mirrory) = 0;
  virtual bool setMode(int x, int y, int binx, int biny, double rate, int xoffset, int yoffset) = 0;
  virtual bool setCompanding(bool activated) = 0;
  virtual MT9VImagerPtr getAlternateContext() = 0;
  virtual bool setBlackLevel(bool manual_override, int calibration_value, int step_size, int filter_length) = 0;
  virtual std::string getModel() = 0;
  virtual uint16_t getVersion() = 0;
  
  static MT9VImagerPtr getInstance(IpCamList &cam);
};

#endif
