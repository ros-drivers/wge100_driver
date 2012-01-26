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

// TODO: doxygen mainpage

// TODO: Timeout receiving packet.
// TODO: Check that partial EOF missing frames get caught.
// @todo Do the triggering based on a stream of incoming timestamps.

#include <ros/node_handle.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/fill_image.h>
#include <sensor_msgs/SetCameraInfo.h>
#include <driver_base/SensorLevels.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/update_functions.h>
#include <diagnostic_updater/publisher.h>
#include <self_test/self_test.h>
#include <stdlib.h>
#include <limits>
#include <math.h>
#include <linux/sysctl.h>
#include <iostream>
#include <fstream>
#include <wge100_camera/BoardConfig.h>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
#include <driver_base/driver.h>
#include <driver_base/driver_node.h>
#include <timestamp_tools/trigger_matcher.h>
#include <camera_calibration_parsers/parse.h>
#include <image_transport/image_transport.h>
#include <sstream>

#include <wge100_camera/WGE100CameraConfig.h>

#include "wge100_camera/wge100lib.h"
#include "wge100_camera/host_netutil.h"
#include "wge100_camera/mt9v.h"

#define RMEM_MAX_RECOMMENDED 20000000

// @todo this should come straight from camera_calibration_parsers as soon as a release happens.
struct SimpleMatrix
{
  int rows;
  int cols;
  const double* data;

  SimpleMatrix(int rows, int cols, const double* data)
    : rows(rows), cols(cols), data(data)
  {}
};

std::ostream& operator << (std::ostream& out, const SimpleMatrix& m)
{
  for (int i = 0; i < m.rows; ++i) {
    for (int j = 0; j < m.cols; ++j) {
      out << m.data[m.cols*i+j] << " ";
    }
    out << std::endl;
  }
  return out;
}

bool writeCalibrationIni(std::ostream& out, const std::string& camera_name,
                         const sensor_msgs::CameraInfo& cam_info)
{
  out.precision(5);
  out << std::fixed;
  
  out << "# Camera intrinsics\n\n";
  /// @todo time?
  out << "[image]\n\n";
  out << "width\n" << cam_info.width << "\n\n";
  out << "height\n" << cam_info.height << "\n\n";
  out << "[" << camera_name << "]\n\n";

  out << "camera matrix\n"     << SimpleMatrix(3, 3, &cam_info.K[0]);
  out << "\ndistortion\n"      << SimpleMatrix(1, 5, &cam_info.D[0]);
  out << "\n\nrectification\n" << SimpleMatrix(3, 3, &cam_info.R[0]);
  out << "\nprojection\n"      << SimpleMatrix(3, 4, &cam_info.P[0]);

  return true;
}




// The FrameTimeFilter class takes a stream of image arrival times that
// include time due to system load and network asynchrony, and generates a
// (hopefully) more steady stream of arrival times. The filtering is based
// on the assumption that the frame rate is known, and that often the time
// stamp is correct. The general idea of the algorithm is:
//
// anticipated_time_ = previous time stamp + frame_period_
// is a good estimate of the current frame time stamp.
//
// Take the incoming time stamp, or the anticipated_time_, whichever one is
// lower. The rationale here is that when the latency is low or zero, the
// incoming time stamp is correct and will dominate. If latency occurs, the
// anticipated_time_ will be used.
//
// To avoid problems with clock skew, max_skew indicates the maximum
// expected clock skew. frame_period_ is set to correspond to the 
// slowest expected rate when clock skew is taken into account. If
// frame_period_ was less than the actual frame rate, then
// anticipated_time_ would always dominate, and the output time stamps
// would slowly diverge from the actual time stamps.
//
// Because the frame rate may sometimes skip around in an unanticipated way
// the filter detects if anticipated_time_ has dominated by more than
// locked_threshold_ more than max_recovery_frames_ in a row. In that case,
// it picks the lowest latency input value that occurs during the
// max_recovery_frames_ to reset anticipated_time_.
//
// Finally, if the filter misses too many frames in a row, it assumes that
// its anticipated_time is invalid and immediately resets the filter.

class FrameTimeFilter
{
public:  
  FrameTimeFilter(double frame_rate = 1., double late_locked_threshold = 1e-3, double early_locked_threshold = 3e-3, int early_recovery_frames = 5, int max_recovery_frames = 10, double max_skew = 1.001, double max_skipped_frames = 100)
  {
    frame_period_ = max_skew / frame_rate;
    max_recovery_frames_ = max_recovery_frames;
    early_recovery_frames_ = early_recovery_frames;
    late_locked_threshold_ = late_locked_threshold;
    early_locked_threshold_ = early_locked_threshold;
    max_skipped_frames_ = max_skipped_frames;
    last_frame_number_ = 0; // Don't really care about this value.
    reset_filter();
  }
  
  void reset_filter()
  {
    relock_time_ = anticipated_time_ = std::numeric_limits<double>::infinity();
    unlocked_count_ = late_locked_threshold_;
  }

  double run(double in_time, int frame_number)
  {
    double out_time = in_time;
    int delta = (frame_number - last_frame_number_) & 0xFFFF; // Hack because the frame rate currently wraps around too early.

    if (delta > max_skipped_frames_)
    {
      ROS_WARN_NAMED("frame_time_filter", "FrameTimeFilter missed too many frames from #%u to #%u. Resetting.", last_frame_number_, frame_number);
      reset_filter();
    }

    anticipated_time_ += frame_period_ * delta; 
    relock_time_ += frame_period_ * delta;

    if (out_time < relock_time_)
      relock_time_ = out_time;
      
    //ROS_DEBUG("in: %f ant: %f", in_time, anticipated_time_);

    bool early_trigger = false;
    if (out_time < anticipated_time_ - early_locked_threshold_ && finite(anticipated_time_))
    {
      ROS_WARN_NAMED("frame_time_filter", "FrameTimeFilter saw frame #%u was early by %f.", frame_number, anticipated_time_ - out_time);
      early_trigger = true;
    }
    if (out_time < anticipated_time_)
    {
      anticipated_time_ = out_time;
      relock_time_ = std::numeric_limits<double>::infinity();
      unlocked_count_ = 0;
    }
    else if (out_time > anticipated_time_ + late_locked_threshold_)
    {
      unlocked_count_++;
      if (unlocked_count_ > max_recovery_frames_)
      {
        ROS_DEBUG_NAMED("frame_time_filter", "FrameTimeFilter lost lock at frame #%u, shifting by %f.", frame_number, relock_time_ - anticipated_time_);
        anticipated_time_ = relock_time_;
        relock_time_ = std::numeric_limits<double>::infinity();
      }
      //else 
      //  ROS_DEBUG("FrameTimeFilter losing lock at frame #%u, lock %i/%i, off by %f.", frame_number, unlocked_count_, max_recovery_frames_, anticipated_time_ - out_time);
      out_time = anticipated_time_;
    }
    
    last_frame_number_ = frame_number;

    return early_trigger ? -out_time : out_time;
  }
  
private:  
  int max_recovery_frames_;
  int unlocked_count_;
  int last_frame_number_;
  int max_skipped_frames_;
  int early_recovery_frames_;
  double early_locked_threshold_;
  double late_locked_threshold_;
  double anticipated_time_;
  double relock_time_;
  double frame_period_;
};

/** Helper class to decide when to warn that no message has been received
 * for a long time. */

class SlowTriggerFilter
{
private:
  bool has_warned_;
  int before_warn_;
  int before_clear_;
  int countdown_;
   
public:
  SlowTriggerFilter(int before_warn, int before_clear)
  {
    has_warned_ = false;
    before_clear_ = before_clear;
    before_warn_ = before_warn;
    countdown_ = before_warn_;
  }

  bool badFrame()
  {
    if (!has_warned_)
    {
      if (!countdown_--)
      {
        has_warned_ = true;
        countdown_ = before_warn_;
        return true;
      }
    }
    else
      countdown_ = before_clear_;

    return false;
  }

  void goodFrame()
  {
    if (has_warned_)
    {
      if (!countdown_--)
      {
        has_warned_ = false;
        countdown_ = before_clear_;
      }
    }
    else
      countdown_ = before_warn_;
  }
};

class WGE100CameraDriver : public driver_base::Driver
{
  friend class WGE100CameraNode;

public:
  typedef wge100_camera::WGE100CameraConfig Config;
  Config config_;

private:
  // Services
  ros::ServiceClient trig_service_;

  // Video mode
  double desired_freq_;
  
  // Statistics
  int rmem_max_;
  int missed_eof_count_;
  int dropped_packets_;
  int massive_frame_losses_;
  bool dropped_packet_event_;
  int missed_line_count_;
  int trigger_matcher_drop_count_;
  double last_image_time_;
  double driver_start_time_;
  unsigned int last_frame_number_;
  unsigned int last_partial_frame_number_;
  int lost_image_thread_count_;
 
  // Timing
  FrameTimeFilter frame_time_filter_;
  timestamp_tools::TriggerMatcher trigger_matcher_;

  // Link information
  sockaddr localMac_;
  in_addr localIp_;
  
  // Camera information
  std::string hwinfo_;
  IpCamList camera_;
  MT9VImagerPtr imager_;
  MT9VImagerPtr alternate_imager_;
  double last_camera_ok_time_;
  std::string image_encoding_;
  bool next_is_alternate_;
  bool first_frame_;
  bool enable_alternate_;
  bool enable_primary_;

  // Threads
  boost::shared_ptr<boost::thread> image_thread_;

  // Frame function (gets called when a frame arrives).
  typedef boost::function<int(size_t, size_t, uint8_t*, ros::Time, bool)> UseFrameFunction;
  UseFrameFunction useFrame_;
    
  struct ImagerSettings
  {
    int height;
    int width;
    int x_offset;
    int y_offset;
    int horizontal_binning;
    int vertical_binning;
    bool auto_black_level;
    int black_level;
    int black_level_filter_length;
    int black_level_step_size;
    int brightness;
    int gain;
    double exposure;
    double max_exposure;
    double imager_rate;
    bool companding;
    bool auto_gain;
    bool auto_exposure;
    bool mirror_x;
    bool mirror_y;
    bool rotate_180;
  };

  int setImagerSettings(MT9VImager &imager, ImagerSettings &cfg)
  {
    int bintable[5] = { -1, 0, 1, -1, 2 };
    
    if (imager.setMode(cfg.width, cfg.height, bintable[cfg.horizontal_binning], bintable[cfg.vertical_binning], cfg.imager_rate, cfg.x_offset, cfg.y_offset))
    {
      setStatusMessage("Error setting video mode.");
      return 1;
    }

    if (imager.setCompanding(cfg.companding))
    {
      setStatusMessage("Error setting companding mode.");
      return 1;
    }

    if (imager.setAgcAec(cfg.auto_gain, cfg.auto_exposure))
    {
      setStatusMessage("Error setting AGC/AEC mode. Exposure and gain may be incorrect.");
      return 1;
    }

    if (imager.setGain(cfg.gain))
    {
      setStatusMessage("Error setting analog gain.");
      return 1;
    }

    if (imager.setMirror(cfg.mirror_x != cfg.rotate_180, cfg.mirror_y != cfg.rotate_180))
    {
      setStatusMessage("Error setting mirroring properties.");
      return 1;
    }

    if (imager.setExposure(cfg.exposure))
    {  
      setStatusMessage("Error setting exposure.");
      return 1;
    }
    
    if (imager.setBlackLevel(!cfg.auto_black_level, cfg.black_level, cfg.black_level_step_size, 
          cfg.black_level_filter_length))
    {
       setStatusMessage("Error setting black level.");
       return 1;
    }

    if (imager.setBrightness(cfg.brightness))
    {  
      setStatusMessage("Error setting brightness.");
      return 1;
    }
    
    if (imager.setMaxExposure(cfg.max_exposure ? cfg.max_exposure : 0.99 / cfg.imager_rate)) 
    {
      setStatusMessage("Error setting maximum exposure.");
      return 1;
    }

    return 0;
  }

  
public:
  WGE100CameraDriver() :
    trigger_matcher_(5, 60), // Recovers from frame drop in 5 frames, queues at least 1 second of timestamps.
    no_timestamp_warning_filter_(10, 10) // 10 seconds before warning. 10 frames to clear.
  {
    // Create a new IpCamList to hold the camera list
//    wge100CamListInit(&camList);
    
    // Clear statistics
    last_camera_ok_time_ = 0;
    last_image_time_ = 0;
    driver_start_time_ = ros::Time::now().toSec();
    missed_line_count_ = 0;
    trigger_matcher_drop_count_ = 0;
    missed_eof_count_ = 0;
    dropped_packets_ = 0;
    lost_image_thread_count_ = 0;      
    massive_frame_losses_ = 0;
    dropped_packet_event_ = false;
  }
  
  void config_update(const Config &new_cfg, uint32_t level = 0)
  {
    config_ = new_cfg;

    if (config_.horizontal_binning == 3) // 1, 2 and 4 are legal, dynamic reconfigure ensures that 3 is the only bad case.
    {
      config_.horizontal_binning = 2;
      ROS_WARN("horizontal_binning = 3 is invalid. Setting to 2.");
    }
    
    if (config_.vertical_binning == 3) // 1, 2 and 4 are legal, dynamic reconfigure ensures that 3 is the only bad case.
    {
      config_.vertical_binning = 2;
      ROS_WARN("horizontal_binning = 3 is invalid. Setting to 2.");
    }

    desired_freq_ = config_.ext_trig ? config_.trig_rate : config_.imager_rate;

    /// @todo add more checks here for video mode and image offset.
    
    if (!config_.auto_exposure && config_.exposure > 0.99 / desired_freq_)
    {
      ROS_WARN("Exposure (%f s) is greater than 99%% of frame period (%f s). Setting to 99%% of frame period.", 
          config_.exposure, 0.99 / desired_freq_);
      config_.exposure = 0.99 * 1 / desired_freq_;
    }
    /*if (!config_.auto_exposure && config_.exposure > 0.95 / desired_freq_)
    {
      ROS_WARN("Exposure (%f s) is greater than 95%% of frame period (%f s). You may not achieve full frame rate.", 
          config_.exposure, 0.95 / desired_freq_);
    }*/
    if (config_.auto_exposure && config_.max_exposure && config_.max_exposure > 0.99 / desired_freq_)
    {
      ROS_WARN("Maximum exposure (%f s) is greater than 99%% of frame period (%f s). Setting to 99%% of frame period.", 
          config_.max_exposure, 0.99 / desired_freq_);
      config_.max_exposure = 0.99 * 1 / desired_freq_;
    }
    /*if (config_.auto_exposure && config_.max_exposure && config_.max_exposure > 0.95 / desired_freq_)
    {
      ROS_WARN("Maximum exposure (%f s) is greater than 95%% of frame period (%f s). You may not achieve full frame rate.", 
          config_.max_exposure, 0.95 / desired_freq_);
    }*/

    enable_primary_ = (config_.register_set != wge100_camera::WGE100Camera_AlternateRegisterSet);
    enable_alternate_ = (config_.register_set != wge100_camera::WGE100Camera_PrimaryRegisterSet);
  }

  ~WGE100CameraDriver()
  {
//    wge100CamListDelAll(&camList);
    close();
  }

  int get_rmem_max()
  {
    int rmem_max;
    {
      std::ifstream f("/proc/sys/net/core/rmem_max");
      f >> rmem_max;
    }
    return rmem_max;
  }

  void doOpen()
  {
    if (state_ != CLOSED)
    {
      ROS_ERROR("Assertion failing. state_ = %i", state_);
     ROS_ASSERT(state_ == CLOSED);
    }

    ROS_DEBUG("open()");
    
    int retval;
    
    // Check that rmem_max is large enough.
    rmem_max_ = get_rmem_max();
    if (rmem_max_ < RMEM_MAX_RECOMMENDED)
    {
      ROS_WARN_NAMED("rmem_max", "rmem_max is %i. Buffer overflows and packet loss may occur. Minimum recommended value is 20000000. Updates may not take effect until the driver is restarted. See http://www.ros.org/wiki/wge100_camera/Troubleshooting", rmem_max_);
    }

    ROS_DEBUG("Redoing discovery.");
    const char *errmsg;
    int findResult = wge100FindByUrl(config_.camera_url.c_str(), &camera_, SEC_TO_USEC(0.2), &errmsg);

    if (findResult)
    {
      setStatusMessagef("Matching URL %s : %s", config_.camera_url.c_str(), errmsg);
      strncpy(camera_.cam_name, "unknown", sizeof(camera_.cam_name));
      camera_.serial = -1;
      return;
    }
      
    retval = wge100Configure(&camera_, camera_.ip_str, SEC_TO_USEC(0.5));
    if (retval != 0) {
      setStatusMessage("IP address configuration failed");
      return;
    }
    
    ROS_INFO("Configured camera. Complete URLs: serial://%u@%s#%s name://%s@%s#%s",
        camera_.serial, camera_.ip_str, camera_.ifName, camera_.cam_name, camera_.ip_str, camera_.ifName);
    
    camera_.serial = camera_.serial;
    hwinfo_ = camera_.hwinfo;

    // We are going to receive the video on this host, so we need our own MAC address
    if ( wge100EthGetLocalMac(camera_.ifName, &localMac_) != 0 ) {
      setStatusMessagef("Unable to get local MAC address for interface %s", camera_.ifName);
      return;
    }

    // We also need our local IP address
    if ( wge100IpGetLocalAddr(camera_.ifName, &localIp_) != 0) {
      setStatusMessagef("Unable to get local IP address for interface %s", camera_.ifName);
      return;
    }
      
    if (config_.ext_trig && config_.trig_timestamp_topic.empty())
    {
      setStatusMessage("External triggering is selected, but no \"trig_timestamp_topic\" was specified.");
      return;
    }

    // Select data resolution
    if ( wge100ImagerSetRes( &camera_, config_.width, config_.height ) != 0) {
      setStatusMessage("Error selecting image resolution.");
      return;
    }

    ImagerSettings primary_settings, alternate_settings;

    primary_settings.height = config_.height;
    primary_settings.width = config_.width;
    primary_settings.x_offset = config_.x_offset;
    primary_settings.y_offset = config_.y_offset;
    primary_settings.horizontal_binning = config_.horizontal_binning;
    primary_settings.vertical_binning = config_.vertical_binning;
    primary_settings.imager_rate = config_.imager_rate;
    primary_settings.companding = config_.companding;
    primary_settings.auto_gain = config_.auto_gain;
    primary_settings.gain = config_.gain;
    primary_settings.auto_exposure = config_.auto_exposure;
    primary_settings.exposure = config_.exposure;
    primary_settings.mirror_x = config_.mirror_x;
    primary_settings.mirror_y = config_.mirror_y;
    primary_settings.rotate_180 = config_.rotate_180;
    primary_settings.auto_black_level = config_.auto_black_level;
    primary_settings.black_level_filter_length = config_.black_level_filter_length;
    primary_settings.black_level_step_size = config_.black_level_step_size;
    primary_settings.black_level = config_.black_level;
    primary_settings.brightness = config_.brightness;
    primary_settings.max_exposure = config_.max_exposure;
    
    alternate_settings.height = config_.height;
    alternate_settings.width = config_.width;
    alternate_settings.x_offset = config_.x_offset;
    alternate_settings.y_offset = config_.y_offset;
    alternate_settings.horizontal_binning = config_.horizontal_binning;
    alternate_settings.vertical_binning = config_.vertical_binning;
    alternate_settings.imager_rate = config_.imager_rate;
    alternate_settings.companding = config_.companding_alternate;
    alternate_settings.auto_gain = config_.auto_gain_alternate;
    alternate_settings.gain = config_.gain_alternate;
    alternate_settings.auto_exposure = config_.auto_exposure_alternate;
    alternate_settings.exposure = config_.exposure_alternate;
    alternate_settings.mirror_x = config_.mirror_x;
    alternate_settings.mirror_y = config_.mirror_y;
    alternate_settings.rotate_180 = config_.rotate_180;
    alternate_settings.auto_black_level = config_.auto_black_level;
    alternate_settings.black_level = config_.black_level;
    alternate_settings.black_level_filter_length = config_.black_level_filter_length;
    alternate_settings.black_level_step_size = config_.black_level_step_size;
    alternate_settings.brightness = config_.brightness;
    alternate_settings.max_exposure = config_.max_exposure;
    
    imager_ = MT9VImager::getInstance(camera_);
    if (!imager_)
    {
      setStatusMessage("Unknown imager model.");
      return;
    }
    alternate_imager_ = imager_->getAlternateContext();
   
    if (enable_primary_ && enable_alternate_) // Both
    {
      if (!alternate_imager_)
      {
        setStatusMessage("Unable to find alternate imager context, but enable_alternate is true.");
        return;
      }

      // Workaround for firmware/imager bug.
      primary_settings.auto_exposure = config_.auto_exposure_alternate;
      primary_settings.gain = config_.gain_alternate;
      primary_settings.companding = config_.companding_alternate;
      alternate_settings.auto_exposure = config_.auto_exposure;
      alternate_settings.gain = config_.gain;
      alternate_settings.companding = config_.companding;
      
      if (setImagerSettings(*imager_, primary_settings) ||
          setImagerSettings(*alternate_imager_, alternate_settings))
        return;
    }
    else if (enable_primary_) // Primary only
    {
      if (setImagerSettings(*imager_, primary_settings))
        return;
    }
    else // Alternate only
    {
      if (setImagerSettings(*imager_, alternate_settings))
        return;
    }

    if (enable_alternate_ && enable_primary_ && wge100SensorSelect(&camera_, 0, 0x07))
    {
      setStatusMessage("Error configuring camera to report alternate frames.");
      return;
    }
      
    next_is_alternate_ = !enable_primary_;
    
    // Initialize frame time filter.
    frame_time_filter_ = FrameTimeFilter(desired_freq_, 0.001, 0.5 / config_.imager_rate);
    trigger_matcher_.setTrigDelay(1. / config_.imager_rate);
    
    // The modulo below is for cameras that got programmed with the wrong
    // serial number. There should only be one currently.
    bool is_027 = ((camera_.serial / 100000) % 100) == 27;
    bool is_MT9V032 = imager_->getModel() == "MT9V032";
    if (camera_.serial != 0 && is_027 == is_MT9V032)
    {
      boost::recursive_mutex::scoped_lock lock(mutex_);
      setStatusMessagef("Camera has %s serial number, but has %s imager. This is a serious problem with the hardware.", 
          is_027 ? "027" : "non-027", imager_->getModel().c_str());
      return;
    }
    
    if (config_.test_pattern && (
          wge100ReliableSensorWrite( &camera_, 0x7F, 0x3800, NULL ) != 0 ||
          wge100ReliableSensorWrite( &camera_, 0x0F, 0x0006, NULL ) != 0)) {
      setStatusMessage("Error turning on test pattern.");
      return;
    }
    
    setStatusMessage("Camera is opened.");
    state_ = OPENED;
    last_camera_ok_time_ = ros::Time::now().toSec(); // If we get here we are communicating with the camera well.
  }

  void doClose()
  {
    ROS_DEBUG("close()");
    ROS_ASSERT(state_ == OPENED);
    state_ = CLOSED;
    setStatusMessage("Camera is closed.");
  }

  void doStart()
  {
    last_frame_number_ = 0xFFFF;
    last_partial_frame_number_ = 0xFFFF;

    // Select trigger mode.
    int trig_mode = config_.ext_trig ? TRIG_STATE_EXTERNAL : TRIG_STATE_INTERNAL;
    if (enable_alternate_ && enable_primary_)
      trig_mode |= TRIG_STATE_ALTERNATE;
    if (config_.rising_edge_trig)
      trig_mode |= TRIG_STATE_RISING;
    if ( wge100TriggerControl( &camera_, trig_mode ) != 0) {
      setStatusMessagef("Trigger mode set error. Is %s accessible from interface %s? (Try running route to check.)", camera_.ip_str, camera_.ifName);
      state_ = CLOSED; // Might have failed because camera was reset. Go to closed.
      return;
    }

    trigger_matcher_.reset();

    /*
    if (0) // Use this to dump register values.
    {
      for (int i = 0; i <= 255; i++)
      {
        uint16_t value;              
        if (wge100ReliableSensorRead(&camera_, i, &value, NULL))
          exit(1);
        printf("usleep(50000); stcam_->setRegister(0xFF000, 0x4%02x%04x); printf(\"%08x\\n\", stcam_->getRegister(0xFF000));\n", i, value);
      }
    } */

    ROS_DEBUG("start()");
    ROS_ASSERT(state_ == OPENED);
    setStatusMessage("Waiting for first frame...");
    state_ = RUNNING;   
    image_thread_.reset(new boost::thread(boost::bind(&WGE100CameraDriver::imageThread, this)));
  }

  void doStop()
  {
    ROS_DEBUG("stop()");
    if (state_ != RUNNING) // RUNNING can exit asynchronously.
      return;
    
    /// @todo race condition here. Need to think more.
    setStatusMessage("Stopped");

    state_ = OPENED;

    if (image_thread_ && !image_thread_->timed_join((boost::posix_time::milliseconds) 2000))
    {
      ROS_ERROR("image_thread_ did not die after two seconds. Pretending that it did. This is probably a bad sign.");
      lost_image_thread_count_++;
    }
    image_thread_.reset();
  }
  
  int setTestMode(uint16_t mode, diagnostic_updater::DiagnosticStatusWrapper &status)
  {
    if ( wge100ReliableSensorWrite( &camera_, 0x7F, mode, NULL ) != 0) {
      status.summary(2, "Could not set imager into test mode.");
      status.add("Writing imager test mode", "Fail");
      return 1;
    }
    else
    {
      status.add("Writing imager test mode", "Pass");
    }

    usleep(100000);
    uint16_t inmode;
    if ( wge100ReliableSensorRead( &camera_, 0x7F, &inmode, NULL ) != 0) {
      status.summary(2, "Could not read imager mode back.");
      status.add("Reading imager test mode", "Fail");
      return 1;
    }
    else
    {
      status.add("Reading imager test mode", "Pass");
    }
    
    if (inmode != mode) {
      status.summary(2, "Imager test mode read back did not match.");
      status.addf("Comparing read back value", "Fail (%04x != %04x)", inmode, mode);
      return 1;
    }
    else
    {
      status.add("Comparing read back value", "Pass");
    }
    
    return 0;
  }

  std::string getID()
  {
    if (!camera_.serial)
      return "unknown";
    else
      return (boost::format("WGE100_%05u") % camera_.serial).str();
  }

private:
  void imageThread()
  {
    // Start video
    frame_time_filter_.reset_filter();

    first_frame_ = true;
    int ret_val = wge100VidReceiveAuto( &camera_, config_.height, config_.width, &WGE100CameraDriver::frameHandler, this);
    
    if (ret_val == IMAGER_LINENO_OVF)
      setStatusMessage("Camera terminated streaming with an overflow error.");
    else if (ret_val == IMAGER_LINENO_ERR)
      setStatusMessage("Camera terminated streaming with an error.");
    else if (ret_val)
      setStatusMessage("wge100VidReceiveAuto exited with an error code.");
    
    // At this point we are OPENED with lock held, or RUNNING, or image_thread_ did not die after two seconds (which should not happen).
    // Thus, it is safe to switch to OPENED, except if the image_thread_
    // did not die, in which case the if will save us most of the time.
    // (Can't take the lock because the doStop thread might be holding it.)
    if (state_ == RUNNING)
      state_ = OPENED;
    ROS_DEBUG("Image thread exiting.");
  }

  void cameraStatus(diagnostic_updater::DiagnosticStatusWrapper& stat)
  {
    double since_last_frame_ = ros::Time::now().toSec() - (last_image_time_ ? last_image_time_ : driver_start_time_);
    if (since_last_frame_ > 10)
    {
      stat.summary(2, "Next frame is way past due.");
    }
    else if (since_last_frame_ > 5 / desired_freq_)
    {
      stat.summary(1, "Next frame is past due.");
    }
    else if (rmem_max_ < RMEM_MAX_RECOMMENDED)
    {
      stat.summaryf(1, "Frames are streaming, but the receive buffer is small (rmem_max=%u). Please increase rmem_max to %u to avoid dropped frames. For details: http://www.ros.org/wiki/wge100_camera/Troubleshooting", rmem_max_, RMEM_MAX_RECOMMENDED);
    }
    else if (dropped_packet_event_ && config_.packet_debug)
    {
      // Only warn in diagnostics for dropped packets if we're in debug mode, #3834
      stat.summary(diagnostic_msgs::DiagnosticStatus::WARN, "There have been dropped packets.");
      dropped_packet_event_ = 0;
    }
    else
    {
      stat.summary(diagnostic_msgs::DiagnosticStatus::OK, "Frames are streaming.");
      dropped_packet_event_ = 0;
    }
    
    stat.addf("Missing image line frames", "%d", missed_line_count_);
    stat.addf("Missing EOF frames", "%d", missed_eof_count_);
    stat.addf("Dropped packet estimate", "%d", dropped_packets_);
    stat.addf("Frames dropped by trigger filter", "%d", trigger_matcher_drop_count_);
    stat.addf("Massive frame losses", "%d", massive_frame_losses_);
    stat.addf("Losses of image thread", "%d", lost_image_thread_count_);
    stat.addf("First packet offset", "%d", config_.first_packet_offset);
    if (isClosed())
    {
      static const std::string not_opened = "not_opened";
      stat.add("Camera Serial #", not_opened);
      stat.add("Camera Name", not_opened);
      stat.add("Camera Hardware", not_opened);
      stat.add("Camera MAC", not_opened);
      stat.add("Interface", not_opened);
      stat.add("Camera IP", not_opened);
      stat.add("Image encoding", not_opened);
    }
    else
    {
      stat.add("Camera Serial #", camera_.serial);
      stat.add("Camera Name", camera_.cam_name);
      stat.add("Camera Hardware", hwinfo_);
      stat.addf("Camera MAC", "%02X:%02X:%02X:%02X:%02X:%02X", camera_.mac[0], camera_.mac[1], camera_.mac[2], camera_.mac[3], camera_.mac[4], camera_.mac[5]);
      stat.add("Interface", camera_.ifName);
      stat.add("Camera IP", camera_.ip_str);
      stat.add("Image encoding", image_encoding_);
    }
    stat.add("Image width", config_.width);
    stat.add("Image height", config_.height);
    stat.add("Image horizontal binning", config_.horizontal_binning);
    stat.add("Image vertical binning", config_.vertical_binning);
    stat.add("Requested imager rate", config_.imager_rate);
    stat.add("Latest frame time", last_image_time_);
    stat.add("Latest frame #", last_frame_number_);
    stat.add("External trigger controller", config_.trig_timestamp_topic);
    stat.add("Trigger mode", config_.ext_trig ? "external" : "internal");
    boost::shared_ptr<MT9VImager> imager = imager_; // For thread safety.
    if (imager)
    {
      stat.add("Imager model", imager->getModel());
      stat.addf("Imager version", "0x%04x", imager->getVersion());
    }
  }

  double getTriggeredFrameTime(double firstPacketTime)
  {
    /*// Assuming that there was no delay in receiving the first packet,
    // this should tell us the time at which the first trigger after the
    // image exposure occurred.
    double pulseStartTime = 0;
      controller::TriggerController::getTickStartTimeSec(firstPacketTime, trig_req_);

    // We can now compute when the exposure ended. By offsetting to the
    // falling edge of the pulse, going back one pulse duration, and going
    // forward one camera frame time.
    double exposeEndTime = pulseStartTime + 
      controller::TriggerController::getTickDurationSec(trig_req_) +
      1 / imager_freq_ -
      1 / config_.trig_rate;
    
    return exposeEndTime;*/
    return 0;
  }

  double getExternallyTriggeredFrameTime(double firstPacketTime)
  {
    // We can now compute when the exposure ended. By offsetting by the
    // empirically determined first packet offset, going back one pulse
    // duration, and going forward one camera frame time.
    
    double exposeEndTime = firstPacketTime - config_.first_packet_offset + 
      1 / config_.imager_rate - 
      1 / config_.trig_rate;

    return exposeEndTime;
  }

  double getFreeRunningFrameTime(double firstPacketTime)
  {
    // This offset is empirical, but fits quite nicely most of the time.
    
    return firstPacketTime - config_.first_packet_offset;
  }
  
//    double nowTime = ros::Time::now().toSec();
//    static double lastExposeEndTime;
//    static double lastStartTime;
//    if (fabs(exposeEndTime - lastExposeEndTime - 1 / trig_rate_) > 1e-4) 
//      ROS_INFO("Mistimed frame #%u first %f first-pulse %f now-first %f pulse-end %f end-last %f first-last %f", eofInfo->header.frame_number, frameStartTime, frameStartTime - pulseStartTime, nowTime - pulseStartTime, pulseStartTime - exposeEndTime, exposeEndTime - lastExposeEndTime, frameStartTime - lastStartTime);
//    lastStartTime = frameStartTime;
//    lastExposeEndTime = exposeEndTime;
//    
//    static double lastImageTime;
//    static double firstFrameTime;
//    if (eofInfo->header.frame_number == 100)
//      firstFrameTime = imageTime;
//    ROS_INFO("Frame #%u time %f ofs %f ms delta %f Hz %f", eofInfo->header.frame_number, imageTime, 1000 * (imageTime - firstFrameTime - 1. / (29.5/* * 0.9999767*/) * (eofInfo->header.frame_number - 100)), imageTime - lastImageTime, 1. / (imageTime - lastImageTime));
//    lastImageTime = imageTime;

  SlowTriggerFilter no_timestamp_warning_filter_;

  int frameHandler(wge100FrameInfo *frame_info)
  {
    boost::recursive_mutex::scoped_lock(diagnostics_lock_);
    
    if (state_ != RUNNING)
      return 1;
    
    if (frame_info == NULL)
    {
      boost::recursive_mutex::scoped_lock lock(mutex_);
      // The select call in the driver timed out.
      /// @TODO Do we want to support rates less than 1 Hz?
      if (config_.ext_trig && !trigger_matcher_.hasTimestamp())
      {
        const char *msg = "More than one second elapsed without receiving any trigger. Is this normal?";
        if (no_timestamp_warning_filter_.badFrame())
          ROS_WARN_NAMED("slow_trigger", "%s", msg);
        else
          ROS_DEBUG("%s", msg);
        return 0;
      }

      /// @todo Could a deadlock happen here if the thread is
      // simultaneously trying to be killed? The other thread will time out
      // on the join, but the error message it spits out in that case is
      // ugly.
      setStatusMessagef("No data have arrived for more than one second. Assuming that camera is no longer streaming.");
      return 1;
    }
    
    no_timestamp_warning_filter_.goodFrame();
    
    if (first_frame_)
    {
      ROS_INFO_COND(first_frame_, "Camera streaming.");
      setStatusMessage("Camera streaming.");
    }
    first_frame_ = false;
    
    double firstPacketTime = frame_info->startTime.tv_sec + frame_info->startTime.tv_usec / 1e6;
      
    // Use a minimum filter to eliminate variations in delay from frame
    // to frame.
    double firstPacketTimeFiltered = fabs(frame_time_filter_.run(firstPacketTime, frame_info->frame_number));
    
    // If we don't know the trigger times then use the arrival time of the
    // first packet run through a windowed minimum filter to estimate the
    // trigger times.

    ros::Time imageTime;
    if (!config_.ext_trig || config_.trig_timestamp_topic.empty())
    {
      // Suppose that the first packet arrived a fixed time after the
      // trigger signal for the next frame. (I.e., this guess will be used
      // as a timestamp for the next frame not for this one.)
      double triggerTimeGuess = firstPacketTimeFiltered - config_.first_packet_offset;
      // Send the trigger estimate to the trigger matcher.
      trigger_matcher_.triggerCallback(triggerTimeGuess);
      //ROS_INFO("Added filtered trigger: %f", triggerTimeGuess);
      //ROS_INFO("Using firstPacketTime: %f", firstPacketTime);
      imageTime = trigger_matcher_.getTimestampNoblock(ros::Time(firstPacketTime));
      //ROS_INFO("Got timestamp: %f delta: %f", imageTime.toSec(), imageTime.toSec() - triggerTimeGuess);
    }     
    else
    {
      //ROS_INFO("Using firstPacketTime: %f", firstPacketTime);
      //double pre = ros::Time::now().toSec();
      imageTime = trigger_matcher_.getTimestampBlocking(ros::Time(firstPacketTime), 0.01);
      //double post = ros::Time::now().toSec();
      //ROS_INFO("Got timestamp: %f delta: %f delay: %f", imageTime.toSec(), imageTime.toSec() - firstPacketTime, post - pre);
    }

    //static double lastunfiltered;
    //ROS_DEBUG("first_packet %f unfilteredImageTime %f frame_id %u deltaunf: %f", frameStartTime, unfilteredImageTime, frame_info->frame_number, unfilteredImageTime - lastunfiltered);
    //lastunfiltered = unfilteredImageTime;
    
    int16_t frame_delta = (frame_info->frame_number - last_partial_frame_number_) & 0xFFFF;
    last_partial_frame_number_ = frame_info->frame_number;
    if (frame_delta > 1)
    {
      dropped_packet_event_ = true;
      ROS_DEBUG_NAMED("dropped_frame", "Some frames were never seen. The dropped packet count will be incorrect.");
      massive_frame_losses_++;
    }

    // Check for frame with missing EOF.
    if (frame_info->eofInfo == NULL) {
      missed_eof_count_++;
      dropped_packets_++;
      dropped_packet_event_ = true;
      ROS_DEBUG_NAMED("no_eof", "Frame %u was missing EOF", frame_info->frame_number);
      return 0;
    }

    // Check for short packet (video lines were missing)
    if (frame_info->shortFrame) {
      missed_line_count_++;
      dropped_packets_ += frame_info->missingLines;
      dropped_packet_event_ = true;
      ROS_DEBUG_NAMED("short_frame", "Short frame #%u (%zu video lines were missing, last was %zu)", frame_info->frame_number, 
          frame_info->missingLines, frame_info->lastMissingLine);
      return 0;
    }

    if (imageTime == timestamp_tools::TriggerMatcher::RetryLater)
    {
      trigger_matcher_drop_count_++;
      ROS_DEBUG_NAMED("missing_timestamp", "TriggerMatcher did not get timestamp for frame #%u", frame_info->frame_number);
      return 0;
    }

    if (imageTime == timestamp_tools::TriggerMatcher::DropData)
    {
      trigger_matcher_drop_count_++;
      ROS_DEBUG_NAMED("trigger_match_fail", "TriggerMatcher dropped frame #%u", frame_info->frame_number);
      return 0;
    }

    last_image_time_ = imageTime.toSec();
    last_frame_number_ = frame_info->frame_number;
    
    image_encoding_ = isColor() ? sensor_msgs::image_encodings::BAYER_BGGR8 : sensor_msgs::image_encodings::MONO8;

    bool alternate = next_is_alternate_;
    next_is_alternate_ = enable_alternate_ && (!enable_primary_ || (frame_info->eofInfo->i2c[0] & 0x8000));

    return useFrame_(frame_info->width, frame_info->height, frame_info->frameData, imageTime, alternate);
  }

  bool isColor()
  {
    return imager_->getModel() == "MT9V032";
// Serial number test fails because of the exceptions I have introduced.
// May want to go back to this test if we get dual register set color
// imagers.
//    return !(camera_.serial >= 2700000 && camera_.serial < 2800000);
  }

  static int frameHandler(wge100FrameInfo *frameInfo, void *userData)
  {
    WGE100CameraDriver &fa_node = *(WGE100CameraDriver*)userData;
    return fa_node.frameHandler(frameInfo);
  }

  uint16_t intrinsicsChecksum(uint16_t *data, int words)
  {
    uint16_t sum = 0;
    for (int i = 0; i < words; i++)
      sum += htons(data[i]);
    return htons(0xFFFF - sum);
  }

  bool loadIntrinsics(sensor_msgs::CameraInfo &cam_info)
  {
    // Retrieve contents of user memory
    std::string calbuff(2 * FLASH_PAGE_SIZE, 0);

    if(wge100ReliableFlashRead(&camera_, FLASH_CALIBRATION_PAGENO, (uint8_t *) &calbuff[0], NULL) != 0 ||
        wge100ReliableFlashRead(&camera_, FLASH_CALIBRATION_PAGENO+1, (uint8_t *) &calbuff[FLASH_PAGE_SIZE], NULL) != 0)
    {
      ROS_WARN("Error reading camera intrinsics from flash.\n");
      return false;
    }

    uint16_t chk = intrinsicsChecksum((uint16_t *) &calbuff[0], calbuff.length() / 2);
    if (chk)
    {
      ROS_WARN_NAMED("no_intrinsics", "Camera intrinsics have a bad checksum. They have probably never been set.");
      return false;
    }                                             

    std::string name;
    bool success = camera_calibration_parsers::parseCalibration(calbuff, "ini", name, cam_info);
    // @todo: rescale intrinsics as needed?
    if (success && (cam_info.width != (unsigned int) config_.width || cam_info.height != (unsigned int) config_.height)) {
      ROS_ERROR("Image resolution from intrinsics file does not match current video setting, "
               "intrinsics expect %ix%i but running at %ix%i", cam_info.width, cam_info.height, config_.width, config_.height);
    }

    if (!success)
      ROS_ERROR("Could not parse camera intrinsics downloaded from camera.");

    return success;
  }

  bool saveIntrinsics(const sensor_msgs::CameraInfo &cam_info)
  {
    char calbuff[2 * FLASH_PAGE_SIZE];

    std::stringstream inifile;
    writeCalibrationIni(inifile, camera_.cam_name, cam_info);
    strncpy(calbuff, inifile.str().c_str(), 2 * FLASH_PAGE_SIZE - 2);

    uint16_t *chk = ((uint16_t *) calbuff) + FLASH_PAGE_SIZE - 1;
    *chk = 0;
    *chk = intrinsicsChecksum((uint16_t *) calbuff, sizeof(calbuff) / 2);

    ROS_INFO("Writing camera info:\n%s", calbuff);
    
    boost::recursive_mutex::scoped_lock lock_(mutex_); // Don't want anybody restarting us behind our back.
    goOpened();
    
    // Retrieve contents of user memory

    bool ret = false;
    if (state_ != OPENED)
      ROS_ERROR("Error putting camera into OPENED state to write to flash.\n");
    else if(wge100ReliableFlashWrite(&camera_, FLASH_CALIBRATION_PAGENO, (uint8_t *) calbuff, NULL) != 0 ||
        wge100ReliableFlashWrite(&camera_, FLASH_CALIBRATION_PAGENO+1, (uint8_t *) calbuff + FLASH_PAGE_SIZE, NULL) != 0)
      ROS_ERROR("Error writing camera intrinsics to flash.\n");
    else   
    {
      ROS_INFO("Camera_info successfully written to camera.");
      ret = true;
    }

    goClosed(); // Need to close to reread the intrinsics.
    goRunning(); // The new intrinsics will get read here.
    return ret;
  }

  bool boardConfig(wge100_camera::BoardConfig::Request &req, wge100_camera::BoardConfig::Response &rsp)
  {
    if (state_ != RUNNING)
    {
      ROS_ERROR("board_config sevice called while camera was not running. To avoid programming the wrong camera, board_config can only be called while viewing the camera stream.");
      rsp.success = 0;
      return 1;
    }

    MACAddress mac;
    if (req.mac.size() != 6)
    {
      ROS_ERROR("board_config service called with %zu bytes in MAC. Should be 6.", req.mac.size());
      rsp.success = 0;
      return 1;
    }
    for (int i = 0; i < 6; i++)
      mac[i] = req.mac[i];
    ROS_INFO("board_config called s/n #%i, and MAC %02x:%02x:%02x:%02x:%02x:%02x.", req.serial, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    stop();
    rsp.success = !wge100ConfigureBoard( &camera_, req.serial, &mac);

    if (rsp.success)
      ROS_INFO("board_config succeeded.");
    else
      ROS_INFO("board_config failed.");

    return 1;
  }
  
};

class WGE100CameraNode : public driver_base::DriverNode<WGE100CameraDriver>
{
public:
  WGE100CameraNode(ros::NodeHandle &nh) :
    driver_base::DriverNode<WGE100CameraDriver>(nh),
    camera_node_handle_(nh.resolveName("camera")),
    camera_node_handle_alt_(nh.resolveName("camera_alternate")),
    cam_pub_diag_(cam_pub_.getTopic(),
        diagnostic_,
        diagnostic_updater::FrequencyStatusParam(&driver_.desired_freq_, &driver_.desired_freq_, 0.05), 
        diagnostic_updater::TimeStampStatusParam()),
    config_bord_service_(private_node_handle_.advertiseService("board_config", &WGE100CameraDriver::boardConfig, &driver_)),
    set_camera_info_service_(camera_node_handle_.advertiseService("set_camera_info", &WGE100CameraNode::setCameraInfo, this)),
    wge100_diagnostic_task_("WGE100 Diagnostics", boost::bind(&WGE100CameraDriver::cameraStatus, &driver_, _1)),
    calibrated_(false)
  {
    // Take care not to advertise the same topic twice to work around ros-pkg #4689,
    // advertising reconfigure services twice for image_transport plugins.
    // In future, image_transport should handle that transparently.
    std::string topic = camera_node_handle_.resolveName("image_raw");
    std::string alt_topic = camera_node_handle_alt_.resolveName("image_raw");
    image_transport::ImageTransport it(nh);
    cam_pub_ = it.advertiseCamera(topic, 1);
    if (topic == alt_topic)
      cam_pub_alt_ = cam_pub_;
    else
      cam_pub_alt_ = it.advertiseCamera(alt_topic, 1);
    
    driver_.useFrame_ = boost::bind(&WGE100CameraNode::publishImage, this, _1, _2, _3, _4, _5);
    driver_.setPostOpenHook(boost::bind(&WGE100CameraNode::postOpenHook, this));
  }
  
private:  
  ros::NodeHandle camera_node_handle_;
  ros::NodeHandle camera_node_handle_alt_;
  
  // Publications
  image_transport::CameraPublisher cam_pub_;
  image_transport::CameraPublisher cam_pub_alt_;
  diagnostic_updater::TopicDiagnostic cam_pub_diag_; 
  sensor_msgs::Image image_;
  sensor_msgs::CameraInfo camera_info_;
  ros::ServiceServer config_bord_service_;
  ros::ServiceServer set_camera_info_service_;
  ros::Subscriber trigger_sub_;
  
  diagnostic_updater::FunctionDiagnosticTask wge100_diagnostic_task_;

  // Calibration
  bool calibrated_;

  int publishImage(size_t width, size_t height, uint8_t *frameData, ros::Time t, bool alternate)
  {
    // Raw data is bayer BGGR pattern
    fillImage(image_, driver_.image_encoding_, height, width, width, frameData);
    
    fflush(stdout);
    (alternate ? cam_pub_alt_ : cam_pub_).publish(image_, camera_info_, t);
    cam_pub_diag_.tick(t);

    return 0;
  }
  
  virtual void reconfigureHook(int level)
  {
    ROS_DEBUG("WGE100CameraNode::reconfigureHook called at level %x", level);
      
    image_.header.frame_id = driver_.config_.frame_id;
    camera_info_.header.frame_id = driver_.config_.frame_id;
  }

  void postOpenHook()
  {
    // Try to load camera intrinsics from flash memory
    calibrated_ = driver_.loadIntrinsics(camera_info_);
    if (calibrated_)
      ROS_INFO("Loaded intrinsics from camera.");
    else
    {
      ROS_WARN_NAMED("no_intrinsics", "Failed to load intrinsics from camera");
      camera_info_ = sensor_msgs::CameraInfo();
      camera_info_.width = driver_.config_.width;
      camera_info_.height = driver_.config_.height;
    }
    // Receive timestamps through callback
    if (driver_.config_.ext_trig && !driver_.config_.trig_timestamp_topic.empty())
    {
      void (timestamp_tools::TriggerMatcher::*cb)(const std_msgs::HeaderConstPtr &) = &timestamp_tools::TriggerMatcher::triggerCallback;
      trigger_sub_ = node_handle_.subscribe(driver_.config_.trig_timestamp_topic, 100, cb, &driver_.trigger_matcher_);
    }
    else
      trigger_sub_.shutdown();

    diagnostic_.setHardwareID(driver_.getID());
  }

  virtual void addDiagnostics()
  {
    // Set up diagnostics
    driver_status_diagnostic_.addTask(&wge100_diagnostic_task_);
  }
  
  virtual void addRunningTests()
  {
    self_test_.add( "Streaming Test", this, &WGE100CameraNode::streamingTest);
  }
  
  bool setCameraInfo(sensor_msgs::SetCameraInfo::Request &req, sensor_msgs::SetCameraInfo::Response &rsp)
  {
    sensor_msgs::CameraInfo &cam_info = req.camera_info;
    
    if (driver_.getState() != driver_base::Driver::RUNNING)
    {
      ROS_ERROR("set_camera_info service called while camera was not running. To avoid programming the wrong camera, set_camera_info can only be called while viewing the camera stream.");
      rsp.status_message = "Camera not running. Camera must be running to avoid setting the intrinsics of the wrong camera.";
      rsp.success = false;
      return 1;
    }

    if ((cam_info.width != (unsigned int) driver_.config_.width || cam_info.height != (unsigned int) driver_.config_.height)) {
      ROS_ERROR("Image resolution of camera_info passed to set_camera_info service does not match current video setting, "
               "camera_info contains %ix%i but camera running at %ix%i", cam_info.width, cam_info.height, driver_.config_.width, driver_.config_.height);
      rsp.status_message = (boost::format("Camera_info resolution %ix%i does not match camera resolution %ix%i.")%cam_info.width%cam_info.height%driver_.config_.width%driver_.config_.height).str();
      rsp.success = false;
    }
    
    rsp.success = driver_.saveIntrinsics(cam_info);
    if (!rsp.success)
      rsp.status_message = "Error writing camera_info to flash.";

    return 1;
  }

  virtual void addOpenedTests()
  {
  }
  
  virtual void addStoppedTests()
  {
    ROS_DEBUG("Adding wge100 camera video mode tests.");
    for (int i = 0; i < MT9V_NUM_MODES; i++)
    {
      if (MT9VModes[i].width != 640)
        continue; // Hack, some of the camera firmware doesn't support other modes.
      diagnostic_updater::TaskFunction f = boost::bind(&WGE100CameraNode::videoModeTest, this, i, _1);
      self_test_.add( str(boost::format("Test Pattern in mode %s")%MT9VModes[i].name), f );
    }
  }
  
  void streamingTest(diagnostic_updater::DiagnosticStatusWrapper& status)
  { 
    // Try multiple times to improve test reliability by retrying on failure
    for (int i = 0; i < 10; i++)
    {
      cam_pub_diag_.clear_window();
      sleep(1);

      cam_pub_diag_.run(status);
      if (!status.level)
        break;
      driver_.start();
    }
  }

  class VideoModeTestFrameHandler
  {
    public:
      VideoModeTestFrameHandler(diagnostic_updater::DiagnosticStatusWrapper &status, int imager_id) : got_frame_(false), frame_count_(0), status_(status), imager_id_(imager_id)
      {}

      volatile bool got_frame_;
      int frame_count_;

      int run(size_t width, size_t height, uint8_t *data, ros::Time stamp, bool alternate)
      {
        status_.add("Got a frame", "Pass");

        bool final = frame_count_++ > 10;
        int mode;
        //int error_count = 0;
        switch (imager_id_)
        {
          case 0x1313:
            mode = 0;
            break;
          case 0x1324:
            mode = 2;
            break;
          default:
            ROS_WARN("Unexpected imager_id, autodetecting test pattern for this camera.");
            mode = -1;
            break;
        }
        
        for (size_t y = 0; y < height; y++)
          for (size_t x = 0; x < width; x++, data++)
          {
            std::string msg;
            
            // Disabled modes other than 640 for now...
            //if (width > 320)
              msg = check_expected(x, y, *data, final, mode);
            //else
              //expected = (get_expected(2 * x, 2 * y) + get_expected(2 * x, 2 * y + 1)) / 2;
                                   
            if (msg.empty())
              continue;

            //if (error_count++ < 50)
            //// MT9V034 has some errors, this should ignore them without
            //// compromising the check that there are no solder bridges.
            //  continue; 
            
            if (!final)
            {
              ROS_WARN("Pattern mismatch, retrying...");
              return 0;
            }

            status_.summary(2, msg);
            status_.add("Frame content", msg);
            
            got_frame_ = true;
            return 1;
          }

        status_.addf("Frame content", "Pass");    
        got_frame_ = true;
        return 1;
      }

    private:
      std::string check_expected(int x, int y, uint8_t actual_col, bool final, int &mode)
      {
#define NUM_TEST_PATTERNS 3
        // There seem to be two versions of the imager with different
        // patterns.
        uint8_t col[NUM_TEST_PATTERNS];
        col[0] = (x + 2 * y + 25) / 4;
        if ((x + 1) / 2 + y < 500)
          col[1] = 14 + x / 4;
        else
          col[1] = 0;
        col[2] = (x + 2 * y + 43) / 4; 

        std::string msg;

        if (mode != -1)
        {
          if (col[mode] == actual_col)
            return "";
        
          msg = (boost::format("Unexpected value %u instead of %u at (x=%u, y=%u)")%(int)actual_col%(int)col[mode]%x%y).str();
        }
        else
        {
          for (int i = 0; i < NUM_TEST_PATTERNS; i++)
            if (col[i] == actual_col)
            {
              mode = i;
              return "";
            }
          
          msg = (boost::format("Unexpected value in first pixel %u, expected: %u, %u or %u")%(int)actual_col%(int)col[0]%(int)col[1]%(int)col[2]).str();
        }
            
        ROS_ERROR("%s", msg.c_str());

        return msg;
      }
      diagnostic_updater::DiagnosticStatusWrapper &status_;
      int imager_id_;
  };

  void videoModeTest(int mode, diagnostic_updater::DiagnosticStatusWrapper& status) 
  {
    if (mode < 0 || mode > MT9V_NUM_MODES)
    {
      ROS_ERROR("Tried to call videoModeTest with out of range mode %u.", mode);
      return;
    }
    
    const Config old_config = driver_.config_;
    WGE100CameraDriver::UseFrameFunction oldUseFrame = driver_.useFrame_;

    Config new_config = driver_.config_;
    new_config.width = MT9VModes[mode].width;
    new_config.height = MT9VModes[mode].height;
    new_config.x_offset = 0;
    new_config.y_offset = 0;
    new_config.imager_rate = MT9VModes[mode].fps;
    new_config.ext_trig = 0;
    new_config.register_set = wge100_camera::WGE100Camera_PrimaryRegisterSet;
    new_config.test_pattern = true;
    driver_.config_update(new_config);
    boost::shared_ptr<MT9VImager> imager = driver_.imager_; // For thread safety.
    VideoModeTestFrameHandler callback(status, imager ? driver_.imager_->getVersion() : 0);
    driver_.useFrame_ = boost::bind(&VideoModeTestFrameHandler::run, boost::ref(callback), _1, _2, _3, _4, _5);

    status.name = (boost::format("Pattern Test: %ix%i at %.1f fps.")%new_config.width%new_config.height%new_config.imager_rate).str();
    status.summary(0, "Passed"); // If nobody else fills this, then the test passed.

    int oldcount = driver_.lost_image_thread_count_;
    for (int i = 0; i < 50 && !callback.got_frame_; i++)
    {
      driver_.start(); // In case something goes wrong.
      usleep(100000);
    }
    driver_.close();
    if (!callback.got_frame_)
    {
      ROS_ERROR("Got no frame during pattern test.");
      status.summary(2, "Got no frame during pattern test.");
    }
    if (oldcount < driver_.lost_image_thread_count_)
    {
      ROS_ERROR("Lost the image_thread. This should never happen.");
      status.summary(2, "Lost the image_thread. This should never happen.");
    }
    
    driver_.useFrame_ = oldUseFrame;
    driver_.config_update(old_config);
    
    ROS_INFO("Exiting test %s with status %i: %s", status.name.c_str(), status.level, status.message.c_str());
  }
};

int main(int argc, char **argv)
{ 
  return driver_base::main<WGE100CameraNode>(argc, argv, "wge100_camera");
}
