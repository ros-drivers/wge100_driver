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

// Author: Blaise Gassend

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>

#include <ros/console.h>
#include <ros/time.h>

#include <wge100_camera/ipcam_packet.h>
#include <wge100_camera/host_netutil.h>
#include <wge100_camera/wge100lib.h>
#include <wge100_camera/mt9v.h>

class WGE100Simulator
{
public:
  WGE100Simulator(uint32_t serial) : serial_no_(serial)
  {                      
    in_addr in_addr;
    in_addr.s_addr = 0;
    socket_ = wge100SocketCreate(&in_addr, WG_CAMCMD_PORT);
    if (socket_ == -1)
    {
      ROS_ERROR("Error creating/binding socket: %s", strerror(errno));
      return;
    }
    
    signal(SIGTERM, WGE100Simulator::setExiting);
    reset();
  }

  int run()
  {
    if (socket_ == -1)
      return -1;
  
    while (!exiting_)
    {
      ProcessCommands();
      SendData();
    }

    return 0;
  }

  ~WGE100Simulator()
  {
    if (socket_ != -1)
      close(socket_);
  }

private:
  uint32_t serial_no_;
  int socket_;
  int running_;
  int width_;
  int height_;
  int frame_;
  //ros::Duration period_;
  ros::Time next_frame_time_;
  sockaddr_in data_addr_;
  static const int NUM_IMAGER_REGISTERS = 256;
  static const int IMAGER_REGISTER_UNDEFINED = 1;
  static const int IMAGER_REGISTER_UNSUPPORTED = 2;
  uint16_t imager_registers_[NUM_IMAGER_REGISTERS]; 
  uint8_t imager_register_flags_[NUM_IMAGER_REGISTERS];

  void reset()
  {
    running_ = 0;
    frame_ = 0;
    width_ = 640;
    height_ = 480;
    //period_ = ros::Duration(1/30.);
    next_frame_time_ = ros::Time(0);
    for (int i = 0; i < NUM_IMAGER_REGISTERS; i++)
    {
      imager_registers_[i] = 0;
      imager_register_flags_[i] |= IMAGER_REGISTER_UNDEFINED | IMAGER_REGISTER_UNSUPPORTED;
    }                
    uint8_t supported_registers[] = { 0x7F, 0x06, 0x05,  0x04,  0x03,      0 };
    uint16_t init_value[]         = { 0   , 0x2D, 0x5E, 0x2F0, 0x1E0, 0x1311 };
    for (unsigned int i = 0; i < sizeof(supported_registers) / sizeof(*supported_registers); i++)
    {
      imager_register_flags_[supported_registers[i]] &= ~(IMAGER_REGISTER_UNDEFINED | IMAGER_REGISTER_UNSUPPORTED);
      imager_registers_[supported_registers[i]] = init_value[i];
    }
                                                             
    uint8_t tolerated_registers[] = { 0x18, 0x15, 0x20, 0x21, 0x01, 0x02, 0x0D, 0x70, 0x71, 0x1C, 0xAF, 0x0B, 0x47, 0x48, 0x4C, 0xA5, 0x35, 0xBD };

    for (unsigned int i = 0; i < sizeof(tolerated_registers) / sizeof(*tolerated_registers); i++)
      imager_register_flags_[tolerated_registers[i]] &= ~IMAGER_REGISTER_UNSUPPORTED;
  }
      
  static inline uint8_t get_diag_test_pattern(int x, int y)
  {
    if ((x + 1) / 2 + y < 500)
      return 14 + x / 4;
    else
      return 0;
  }

  void SendData()
  {
    if (!running_)
    {
      next_frame_time_ = ros::Time(0);
      ros::Duration(0.01).sleep();
      return;
    }

    if (width_ != imager_registers_[0x04])
    {
      ROS_ERROR("Width mismatch %i reg: %i", width_, imager_registers_[0x04]);
      ros::Duration(0.01).sleep();
      return;
    }
    
    if (height_ != imager_registers_[0x03])
    {
      ROS_ERROR("Height mismatch %i reg: %i", height_, imager_registers_[0x03]);
      ros::Duration(0.01).sleep();
      return;
    }
    
    ros::Duration sleeptime = (next_frame_time_ - ros::Time::now());
    ros::Duration period = ros::Duration(((imager_registers_[0x03] + imager_registers_[0x06]) * (imager_registers_[0x04] + imager_registers_[0x05]) + 4) / 16e6);
    
    if (sleeptime < -period)
    {
      if (next_frame_time_ != ros::Time(0))
        ROS_WARN("Simulator lost lock on frame #%i", frame_);
      next_frame_time_ = ros::Time::now();
    }
    else
      sleeptime.sleep();
    next_frame_time_ += period;
    
    PacketVideoLine pvl;
    PacketEOF peof;

    peof.header.frame_number = pvl.header.frame_number = htonl(frame_);
    peof.header.horiz_resolution = pvl.header.horiz_resolution = htons(width_);
    peof.header.vert_resolution = pvl.header.vert_resolution = htons(height_);
  
    for (int y = 0; y < height_; y++)
    {
      if ((imager_registers_[0x7F] & 0x3C00) == 0x3800)
      {
        if (width_ > 320)
          for (int x = 0; x < width_; x++)
            pvl.data[x] = get_diag_test_pattern(x, y);
        else
          for (int x = 0; x < width_; x++)
            pvl.data[x] = (get_diag_test_pattern(2 * x, 2 * y) + get_diag_test_pattern(2 * x, 2 * y + 1)) / 2;
      }
      else
      {
        for (int x = 0; x < width_; x++)
          pvl.data[x] = x + y + frame_;
      }

      pvl.header.line_number = htons(y | (frame_ << 10));
      sendto(socket_, &pvl, sizeof(pvl.header) + width_, 0, 
          (struct sockaddr *) &data_addr_, sizeof(data_addr_));
      if (y == 0)
      {
        sched_yield(); // Make sure that first packet arrives with low jitter
        ROS_INFO("Sending frame #%i", frame_);
        fflush(stdout);
      }
    }

    peof.header.line_number = htons(IMAGER_LINENO_EOF);
    peof.i2c_valid = 0;
    peof.i2c[0] = 0;
    sendto(socket_, &peof, sizeof(peof), 0, 
        (struct sockaddr *) &data_addr_, sizeof(data_addr_));
    
    frame_ = (frame_ + 1) & 0xFFFF;
  }

  void ProcessCommands()
  {
    unsigned char buff[100];

    while (true)
    {
      ssize_t len = recv(socket_, buff, 100, MSG_DONTWAIT);
      if (len == -1)
        break;

      PacketGeneric *hdr = (PacketGeneric *) buff;

      if (len == 1 && buff[0] == 0xFF)
        continue; // This is the magic ping packet, just ignore it.

      if (ntohl(hdr->magic_no) != WG_MAGIC_NO ||
          ntohl(hdr->type) > PKT_MAX_ID)
      {
        ROS_INFO("Got a packet with a bad magic number or type."); \
        continue;
      }

#define CAST_AND_CHK(type) \
if (len != sizeof(type)) \
{ \
   ROS_INFO("Got a "#type" with incorrect length"); \
   continue;\
} \
if (false) ROS_INFO("Got a "#type""); \
type *pkt = (type *) buff; 
// End of this horrid macro that makes pkt appear by magic.

      switch (ntohl(hdr->type))
      {
        case PKTT_DISCOVER:
          {
            CAST_AND_CHK(PacketDiscover);
            sendAnnounce(&pkt->hdr);
            break;
          }

        case PKTT_CONFIGURE:
          {
            CAST_AND_CHK(PacketConfigure);
            if (pkt->product_id != htonl(CONFIG_PRODUCT_ID) ||
                pkt->ser_no != htonl(serial_no_))
              break;
            running_ = false;
            sendAnnounce(&pkt->hdr);
            break;
          } 
    
        case PKTT_TRIGCTRL:
          {
            CAST_AND_CHK(PacketTrigControl);
            unsigned int new_mode = htons(pkt->trig_state);
            if (new_mode == 0 && !running_)
              sendStatus(&pkt->hdr, PKT_STATUST_OK, 0);
            else
            {
              ROS_ERROR("Tried to set to trigger mode %i.", new_mode);
              sendStatus(&pkt->hdr, PKT_STATUST_ERROR, PKT_ERROR_INVALID);
            }
            break;
          }

        case PKTT_IMGRMODE:
          {
            CAST_AND_CHK(PacketImagerMode);
            uint32_t mode = htonl(pkt->mode);
            if (mode >= 10 || running_)
            {
              sendStatus(&pkt->hdr, PKT_STATUST_ERROR, PKT_ERROR_INVALID);
              break;
            }
            //double fps[10] = {  15,12.5,  30,  25,  15,12.5,  60,  50,  30,  25 };
            //int widths[10] = { 752, 752, 640, 640, 640, 640, 320, 320, 320, 320 };
            //int heights[10] ={ 480, 480, 480, 480, 480, 480, 240, 240, 240, 240 };
            //period_ = ros::Duration(1/fps[mode]);
            width_ = MT9VModes[mode].width;
            height_ = MT9VModes[mode].height; 
            imager_registers_[0x03] = height_;
            imager_registers_[0x04] = width_;
            imager_registers_[0x05] = MT9VModes[mode].hblank;
            imager_registers_[0x06] = MT9VModes[mode].vblank;
            sendStatus(&pkt->hdr, PKT_STATUST_OK, 0);
          }
          break;

        case PKTT_IMGRSETRES:
          {
            CAST_AND_CHK(PacketImagerSetRes);
            unsigned int new_width = htons(pkt->horizontal);
            unsigned int new_height = htons(pkt->vertical);
            if (new_width > 752 || new_height > 640)
            {
              ROS_ERROR("Tried to set resolution to out of range %ix%i", new_width, new_height);
              sendStatus(&pkt->hdr, PKT_STATUST_ERROR, PKT_ERROR_INVALID);
              break;
            }
            //period_ = ros::Duration(1/10); // @todo fix this.
            width_ = new_width;
            height_ = new_height;
            sendStatus(&pkt->hdr, PKT_STATUST_OK, 0);
          }
          break;

        case PKTT_VIDSTART:
          {
            CAST_AND_CHK(PacketVidStart);
            if (running_)
              sendStatus(&pkt->hdr, PKT_STATUST_ERROR, PKT_ERROR_INVALID);
            else
            {
              sendStatus(&pkt->hdr, PKT_STATUST_OK, 0);
              bzero(&data_addr_, sizeof(data_addr_)); 
              data_addr_.sin_family = AF_INET; 
              data_addr_.sin_addr.s_addr = pkt->receiver.addr; 
              data_addr_.sin_port = pkt->receiver.port; 
            }
            running_ = true;
          }
          break;

        case PKTT_VIDSTOP:
          {
            CAST_AND_CHK(PacketVidStop);
            if (running_)
              sendStatus(&pkt->hdr, PKT_STATUST_OK, 0);
            else
              sendStatus(&pkt->hdr, PKT_STATUST_ERROR, PKT_ERROR_INVALID);
            running_ = false;
          }
          break;

        case PKTT_RESET:
          {
            reset();
          }
          break;

        case PKTT_SENSORRD:
          {
            CAST_AND_CHK(PacketSensorRequest);
            if (imager_register_flags_[pkt->address] & IMAGER_REGISTER_UNDEFINED)
              ROS_WARN("Reading uninitialized imager register 0x%02X.", pkt->address);
            if (imager_register_flags_[pkt->address] & IMAGER_REGISTER_UNSUPPORTED)
              ROS_WARN("Reading unsupported imager register 0x%02X.", pkt->address);
            sendSensorData(&pkt->hdr, pkt->address, imager_registers_[pkt->address]);
          }
          break;

        case PKTT_SENSORWR:
          {
            CAST_AND_CHK(PacketSensorData);
            if (imager_register_flags_[pkt->address] & IMAGER_REGISTER_UNSUPPORTED)
              ROS_ERROR("Writing unsupported imager register 0x%02X.", pkt->address);
            imager_register_flags_[pkt->address] &= ~IMAGER_REGISTER_UNDEFINED;
            imager_registers_[pkt->address] = ntohs(pkt->data);
            sendStatus(&pkt->hdr, PKT_STATUST_OK, 0);
          }
          break;

        case PKTT_FLASHREAD:
          {
            CAST_AND_CHK(PacketFlashRequest);
            sendFlashData(&pkt->hdr, pkt->address);
          }
          break;

        default:
          ROS_ERROR("Got an unknown message type: %i", ntohl(hdr->type));
          break;
      }
#undef CAST_AND_CHK
    }
  }
  
#define FILL_HDR(pkttype, pktid) \
if (false) ROS_INFO("Sending a "#pkttype" packet."); \
Packet##pkttype rsp; \
rsp.hdr.magic_no = htonl(WG_MAGIC_NO); \
strncpy(rsp.hdr.hrt, #pkttype, sizeof(rsp.hdr.hrt)); \
rsp.hdr.type = htonl(pktid);
// End of this horrid macro that makes resp appear by magic.
    
#define SEND_RSP \
sockaddr_in rsp_addr; \
bzero(&rsp_addr, sizeof(rsp_addr)); \
rsp_addr.sin_family = AF_INET; \
rsp_addr.sin_addr.s_addr = hdr->reply_to.addr; \
rsp_addr.sin_port = hdr->reply_to.port; \
sendto(socket_, &rsp, sizeof(rsp), 0, \
    (struct sockaddr *) &rsp_addr, sizeof(rsp_addr)); 

  void sendAnnounce(PacketGeneric *hdr)
  {        
    FILL_HDR(Announce, PKTT_ANNOUNCE); 
    bzero(rsp.mac, sizeof(rsp.mac)); 
    rsp.product_id = htonl(CONFIG_PRODUCT_ID); 
    rsp.ser_no = htonl(serial_no_); 
    strncpy(rsp.product_name, "WGE100_camera_simulator.", sizeof(rsp.product_name)); 
    rsp.hw_version = htonl(0x2041); 
    rsp.fw_version = htonl(0x0112); 
    strncpy(rsp.camera_name, "WGE100_camera_simulator.", sizeof(rsp.camera_name)); 
    SEND_RSP;
  }

  void sendStatus(PacketGeneric *hdr, uint32_t type, uint32_t code)
  {        
    if (type != PKT_STATUST_OK)
      ROS_ERROR("Error Condition.");
    FILL_HDR(Status, PKTT_STATUS); 
    rsp.status_type = htonl(type);
    rsp.status_code = htonl(code);
    SEND_RSP;
  }

  void sendFlashData(PacketGeneric *hdr, uint32_t address)
  {
    FILL_HDR(FlashPayload, PKTT_FLASHDATA);
    rsp.address = address;
    bzero(rsp.data, sizeof(rsp.data));
    SEND_RSP;
  }

  void sendSensorData(PacketGeneric *hdr, uint8_t addr, uint16_t value)
  {        
    FILL_HDR(SensorData, PKTT_SENSORDATA); 
    rsp.address = addr;
    rsp.data = htons(value);
    SEND_RSP;
  }

#undef FILL_HDR
#undef SEND_RSP
  volatile static bool exiting_;
  
  static void setExiting(int i)
  {
    exiting_ = true;
  }
};

volatile bool WGE100Simulator::exiting_ = false;

int main(int argc, char **argv)
{
  ros::Time::init();
  WGE100Simulator wge100sim(12345);
  return wge100sim.run();
}

