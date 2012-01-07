/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009-2010, Willow Garage, Inc.
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

#include <wge100_camera/mt9v.h>
#include <ros/console.h>
#include <math.h>
#include <values.h>

#define VMODEDEF(width, height, fps, hblank, vblank) { #width"x"#height"x"#fps, width, height, fps, hblank, vblank }  
const struct MT9VMode MT9VModes[MT9V_NUM_MODES] = {
  VMODEDEF(752,480,15, 974, 138),
  VMODEDEF(752,480,12.5, 848, 320),
  VMODEDEF(640,480,30, 372, 47),
  VMODEDEF(640,480,25, 543, 61),
  VMODEDEF(640,480,15, 873, 225),
  VMODEDEF(640,480,12.5, 960, 320),
  VMODEDEF(320,240,60, 106, 73),
  VMODEDEF(320,240,50, 180, 80),
  VMODEDEF(320,240,30, 332, 169),
  VMODEDEF(320,240,25, 180, 400)
};

class MT9VImagerImpl : public MT9VImager
{
protected:
  IpCamList &camera_;

  uint8_t reg_column_start_;
  uint8_t reg_row_start_;
  uint8_t reg_window_width_;
  uint8_t reg_window_height_;
  uint8_t reg_hblank_;
  uint8_t reg_vblank_;
  uint8_t reg_shutter_width_;
  uint8_t reg_max_shutter_width_;
  uint8_t reg_analog_gain_;
  uint8_t reg_agc_aec_enable_;
  uint8_t reg_read_mode_;
  uint8_t reg_companding_mode_; 
  uint8_t reg_desired_bin_;

// Imager dependent bounds
  int max_max_shutter_width_;
  int max_shutter_width_;
    
// Some bits are not always at the same position
  int agc_aec_enable_shift_;
  int companding_mode_shift_;

// Cached register values
  uint16_t read_mode_cache_;
  uint16_t *agc_aec_mode_cache_;
  uint16_t agc_aec_mode_cache_backing_;;
  uint16_t *companding_mode_cache_;
  uint16_t companding_mode_cache_backing_;;

  uint16_t imager_version_;
  double line_time_;
 
  std::string model_;

  MT9VImagerPtr alternate_;

public:
  MT9VImagerImpl(IpCamList &cam) : camera_(cam)
  {
    reg_column_start_ = 0x01;
    reg_row_start_ = 0x02;
    reg_window_width_ = 0x04;
    reg_window_height_ = 0x03;
    reg_hblank_ = 0x05;
    reg_vblank_ = 0x06;
    reg_shutter_width_ = 0x0B;
    reg_max_shutter_width_ = 0xBD;
    reg_analog_gain_ = 0x35;
    reg_agc_aec_enable_ = 0xAF;
    reg_read_mode_ = 0x0D;
    reg_companding_mode_ = 0x1C;
    reg_desired_bin_ = 0xA5;

    agc_aec_enable_shift_ = 0;
    companding_mode_shift_ = 0;

    max_max_shutter_width_ = 2047;
    max_shutter_width_ = 32767;

    read_mode_cache_ = 0x300;
    agc_aec_mode_cache_ = &agc_aec_mode_cache_backing_;
    *agc_aec_mode_cache_ = 3;
    companding_mode_cache_ = &companding_mode_cache_backing_;
    *companding_mode_cache_ = 2;

    line_time_ = 0;

    if (wge100ReliableSensorRead(&cam, 0x00, &imager_version_, NULL))
    {
      ROS_WARN("MT9VImager::getInstance Unable to read imager version.");
    }
    //ROS_FATAL("Black level correction is forced off for debugging. This shouldn't get checked in.");
    //setBlackLevel(true, 64, 0, 0);
  }

  virtual ~MT9VImagerImpl()
  {
  }

  virtual bool setAgcAec(bool agc, bool aec)
  {
    uint16_t mask = 3 << agc_aec_enable_shift_;
    uint16_t val = (agc ? 2 : 0) | (aec ? 1 : 0);
    val <<= agc_aec_enable_shift_;

    *agc_aec_mode_cache_ = (*agc_aec_mode_cache_ & ~mask) | val;

    if (wge100ReliableSensorWrite(&camera_, reg_agc_aec_enable_, *agc_aec_mode_cache_, NULL) != 0)
    {
      ROS_WARN("Error setting AGC/AEC mode. Exposure and gain may be incorrect.");
      return true;
    }
    return false;
  }

  virtual bool setBrightness(int brightness)
  {
    if (wge100ReliableSensorWrite(&camera_, reg_desired_bin_, brightness, NULL) != 0)
    {
      ROS_WARN("Error setting brightness.");
      return true;
    }
    return false;
  }

  virtual bool setGain(int gain)
  {
    if (wge100ReliableSensorWrite(&camera_, reg_analog_gain_, 0x8000 | gain, NULL) != 0)
    {
      ROS_WARN("Error setting analog gain.");
      return true;
    }
    return false;
  }

  virtual bool setExposure(double exp)
  {
    if (line_time_ == 0)
    {
      ROS_ERROR("setExposure called before setMode in class MT9VImager. This is a bug.");
      return true;
    }
    int explines = exp / line_time_;
    if (explines < 1) /// @TODO warning here?
    {
      ROS_WARN("Requested exposure too short, setting to %f s", line_time_);
      explines = 1;
    }
    if (explines > max_shutter_width_) /// @TODO warning here?
    {
      explines = max_shutter_width_;
      ROS_WARN("Requested exposure too long, setting to %f s", explines * line_time_);
    }
    ROS_DEBUG("Setting exposure lines to %i (%f s).", explines, explines * line_time_);
    if ( wge100ReliableSensorWrite( &camera_, reg_shutter_width_, explines, NULL) != 0)
    {
      ROS_WARN("Error setting exposure.");
      return true;
    }
    return false;
  }
  
  virtual bool setMaxExposure(double exp)
  {
    if (line_time_ == 0)
    {
      ROS_ERROR("setMaxExposure called before setMode in class MT9VImager. This is a bug.");
      return true;
    }
    int explines = exp / line_time_;
    if (explines < 1) /// @TODO warning here?
    {
      ROS_WARN("Requested max exposure too short, setting to %f s", line_time_);
      explines = 1;
    }
    if (explines > max_max_shutter_width_) /// @TODO warning here?
    {
      explines = max_max_shutter_width_;
      ROS_WARN("Requested max exposure too long, setting to %f s", explines * line_time_);
    }
    ROS_DEBUG("Setting max exposure lines to %i (%f s).", explines, explines * line_time_);
    if ( wge100ReliableSensorWrite( &camera_, reg_max_shutter_width_, explines, NULL) != 0)
    {
      ROS_WARN("Error setting max exposure.");
      return true;
    }
    return false;
  }

  virtual bool setMirror(bool mirrorx, bool mirrory)
  {
    uint16_t mask = 0x30;
    uint16_t val = (mirrory ? 0x10 : 0) | (mirrorx ? 0x20 : 0);

    read_mode_cache_ = (read_mode_cache_ & ~mask) | val;

    if (wge100ReliableSensorWrite(&camera_, reg_read_mode_, read_mode_cache_, NULL) != 0)
    {
      ROS_WARN("Error setting mirror mode. Read mode register settings may have been corrupted.");
      return true;
    }
    return false;
  }
  
  virtual bool setBlackLevel(bool manual_override, int calibration_value, int step_size, int filter_length)
  {
    if (wge100ReliableSensorWrite(&camera_, 0x47, ((filter_length & 7) << 5) | manual_override, NULL) != 0 ||
        wge100ReliableSensorWrite(&camera_, 0x48, calibration_value & 0xFF, NULL) != 0 ||
        wge100ReliableSensorWrite(&camera_, 0x4C, step_size & 0x1F, NULL) != 0)
    {
      ROS_WARN("Error setting black level correction mode.");
      return true;
    }

    return false;
  }

  virtual bool setMode(int x, int y, int binx, int biny, double rate, int xoffset, int yoffset)
  {
    if (binx < 0 || binx > 2)
    {
      ROS_ERROR("x binning should 0, 1 or 2, not %i", binx);
      return true;
    }

    if (biny < 0 || biny > 2)
    {
      ROS_ERROR("y binning should 0, 1 or 2, not %i", biny);
      return true;
    }

    x <<= binx;
    y <<= biny;
    
    if (x < 1 || x > 752)
    {
      ROS_ERROR("Requested x resolution (includes binning) %i is not between 1 and 752.", x);
      return true;
    }

    if (y < 1 || y > 480)
    {
      ROS_ERROR("Requested y resolution (includes binning) %i is not between 1 and 480.", y);
      return true;
    }
    
    // XXX -old center-based calculation left here for reference #4670
    // startx = (752 - x) / 2 + xoffset + 1;
    // starty = (480 - y) / 2 + yoffset + 4;

    int startx, starty;
    startx = 57 + xoffset;
    starty = 4 + yoffset;

    if (startx < 1 || startx + x > 752 + 1)
    {
      ROS_ERROR("Image x offset exceeds imager bounds.");
      return true;
    }

    if (starty < 4 || starty + y > 480 + 4)
    {
      ROS_ERROR("Image y offset exceeds imager bounds.");
      return true;
    }

    int bestvblank = 0;
    int besthblank = 0;
    
    double clock = 16e6;
    int target_tics = clock / rate;
    int besterror = INT_MAX;
    int packet_size = 70 + (x >> binx);
    double max_peak_data_rate = 90e6;
    double best_peak_data_rate = 0;
    int best_tics = 0;
    ROS_DEBUG("Target tics %i", target_tics);

    for (int hblank = 100; hblank < 1023; hblank++) // Smaller min values are possible depending on binning. Who cares...
    {
      int htics = x + hblank;
      int extra_tics = 4;
      int vblank = round(((double) target_tics - extra_tics) / htics) - y;
      if (vblank < 40) // Could probably use 4 here
        vblank = 40;
      if (vblank > 3000) // Some imagers can do more
        vblank = 3000;
      int vtics = vblank + y;
      
      int tics = htics * vtics + extra_tics;
      int err = abs(tics - target_tics);
      double peak_data_rate = 8 * packet_size * clock / htics;

      if (peak_data_rate <= max_peak_data_rate && err < besterror)
      {
        besterror = err;
        best_tics = tics;
        bestvblank = vblank;
        besthblank = hblank;
        best_peak_data_rate = peak_data_rate;
      }
    }

    if (besterror == INT_MAX)
    {
      ROS_ERROR("Could not find suitable vblank and hblank for %ix%i mode.\n", x, y);
      return true;
    }

    line_time_ = (besthblank + x) / clock;

    ROS_DEBUG("Selected vblank=%i hblank=%i data_rate=%f err=%f period=%f", bestvblank, besthblank, best_peak_data_rate, besterror / clock, best_tics / clock);
    
    if (wge100ReliableSensorWrite(&camera_, reg_vblank_, bestvblank, NULL) != 0)
    {
      ROS_ERROR("Failed to set vertical blanking. Stream will probably be corrupt.");
      return true;
    }
    
    if (wge100ReliableSensorWrite(&camera_, reg_hblank_, besthblank, NULL) != 0)
    {
      ROS_ERROR("Failed to set horizontal blanking. Stream will probably be corrupt.");
      return true;
    }
    
    if (wge100ReliableSensorWrite(&camera_, reg_column_start_, startx, NULL) != 0)
    {
      ROS_ERROR("Failed to set start column. Stream will probably be corrupt.");
      return true;
    }
    
    if (wge100ReliableSensorWrite(&camera_, reg_row_start_, starty, NULL) != 0)
    {
      ROS_ERROR("Failed to set start row. Stream will probably be corrupt.");
      return true;
    }
    
    if (wge100ReliableSensorWrite(&camera_, reg_window_width_, x, NULL) != 0)
    {
      ROS_ERROR("Failed to set image width. Stream will probably be corrupt.");
      return true;
    }
    
    if (wge100ReliableSensorWrite(&camera_, reg_window_height_, y, NULL) != 0)
    {
      ROS_ERROR("Failed to set image height. Stream will probably be corrupt.");
      return true;
    }

    uint16_t mask = 0x0F;
    uint16_t val = (binx << 2) | biny;

    read_mode_cache_ = (read_mode_cache_ & ~mask) | val;
    
    if (wge100ReliableSensorWrite(&camera_, reg_read_mode_, read_mode_cache_, NULL) != 0)
    {
      ROS_ERROR("Failed to set binning modes. Read mode may be corrupted.");
      return true;
    }

    return false;
  }
  
  virtual bool setCompanding(bool activated)
  {
    uint16_t mask = 3 << companding_mode_shift_;
    uint16_t val = (activated ? 3 : 2) << companding_mode_shift_;
    *companding_mode_cache_ = (*companding_mode_cache_ & ~mask) | val;

    if (wge100ReliableSensorWrite(&camera_, reg_companding_mode_, *companding_mode_cache_, NULL) != 0)
    {
      ROS_WARN("Error setting companding mode.");
      return true;
    }
    return false;
  }

  virtual MT9VImagerPtr getAlternateContext()
  {
    return alternate_;
  }

  virtual uint16_t getVersion()
  {
    return imager_version_;
  }

  virtual std::string getModel()
  {
    return model_;
  }
};

class MT9V034 : public MT9VImagerImpl
{
public:
  class MT9V034Alternate : public MT9VImagerImpl
  {
  public:
    ~MT9V034Alternate()
    {
    }
    
    MT9V034Alternate(IpCamList &cam, MT9V034 *parent) : MT9VImagerImpl(cam)
    {
      model_ = "MT9V034";
      agc_aec_mode_cache_ = &parent->agc_aec_mode_cache_backing_;
      companding_mode_cache_ = &parent->companding_mode_cache_backing_;
      agc_aec_enable_shift_ = 8;
      companding_mode_shift_ = 8;
      reg_column_start_ = 0xC9;
      reg_row_start_ = 0x0CA;
      reg_window_width_ = 0xCC;
      reg_window_height_ = 0xCB;
      reg_hblank_ = 0xCD;
      reg_vblank_ = 0xCE;
      reg_shutter_width_ = 0xD2;
      reg_max_shutter_width_ = 0xAD; // FIXME Yes, they went and used the same max frame rate for both contexts! Why???
      reg_analog_gain_ = 0x36;
      reg_agc_aec_enable_ = 0xAF;
      reg_read_mode_ = 0x0E;
    }
  };

  MT9V034(IpCamList &cam) : MT9VImagerImpl(cam)
  {
    ROS_DEBUG("Found MT9V034 imager.");
    model_ = "MT9V034";
    max_max_shutter_width_ = 32765;
    max_shutter_width_ = 32765;
    reg_max_shutter_width_ = 0xAD;
    *agc_aec_mode_cache_ = 0x303;
    *companding_mode_cache_ = 0x302;
    alternate_ = MT9VImagerPtr(new MT9V034Alternate(cam, this));

    if (wge100ReliableSensorWrite(&camera_, 0x18, 0x3e3a, NULL) || 
        wge100ReliableSensorWrite(&camera_, 0x15, 0x7f32, NULL) ||
        wge100ReliableSensorWrite(&camera_, 0x20, 0x01c1, NULL) ||
        wge100ReliableSensorWrite(&camera_, 0x21, 0x0018, NULL))
    {
      ROS_WARN("Error setting magic sensor register.");
    }
    if (wge100ReliableSensorWrite(&camera_, 0x70, 0x000, NULL) != 0)
    {
      ROS_WARN("Error turning off row-noise correction.");
    }
    if (wge100ReliableSensorWrite(&camera_, 0x3A, 0x001A, NULL))
    {
      ROS_WARN("Error setting V2 Voltge level for context B.");
    }
    if (wge100ReliableSensorWrite(&camera_, 0x0F, 0x0000, NULL))
    {
      ROS_WARN("Error turning off high dynamic range for alternate configuration.");
    }
    if (wge100ReliableSensorWrite(&camera_, 0xD1, 0x0164, NULL))
    {
      ROS_WARN("Error turning on exposure knee auto-adjust for alternate configuration.");
    }
  }  

  virtual ~MT9V034()
  {
  }
};

class MT9V032 : public MT9VImagerImpl
{
public:
  MT9V032(IpCamList &cam) : MT9VImagerImpl(cam)
  {
    ROS_DEBUG("Found MT9V032 imager.");
    model_ = "MT9V032";
    if (wge100ReliableSensorWrite(&camera_, 0x18, 0x3e3a, NULL) || 
        wge100ReliableSensorWrite(&camera_, 0x15, 0x7f32, NULL) ||
        wge100ReliableSensorWrite(&camera_, 0x20, 0x01c1, NULL) ||
        wge100ReliableSensorWrite(&camera_, 0x21, 0x0018, NULL))
    {
      ROS_WARN("Error setting magic sensor register.");
    }
    if (wge100ReliableSensorWrite(&camera_, 0x70, 0x14, NULL) != 0)
    {
      ROS_WARN("Error turning off row-noise correction");
    }
  }

  virtual ~MT9V032()
  {
  }
};

MT9VImagerPtr MT9VImager::getInstance(IpCamList &cam)
{
  uint16_t imager_version;
  if (wge100ReliableSensorRead(&cam, 0x00, &imager_version, NULL))
  {
    ROS_ERROR("MT9VImager::getInstance Unable to read imager version.");
    return MT9VImagerPtr();
  }

  switch (imager_version)
  {
    case 0x1324:
      return MT9VImagerPtr(new MT9V034(cam));

    default:
      ROS_ERROR("MT9VImager::getInstance Unknown imager version 0x%04x. Assuming it is compatible with MT9V032.", imager_version);
      return MT9VImagerPtr();
    
    case 0x1311:
    case 0x1313:
      return MT9VImagerPtr(new MT9V032(cam));
  }
}


