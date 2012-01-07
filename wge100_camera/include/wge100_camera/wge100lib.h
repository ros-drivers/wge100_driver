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

#ifndef _WGE100LIB_H_
#define _WGE100LIB_H_
#include <stdint.h>
#include "list.h"
#include "ipcam_packet.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Firmware version number
 */
#define WGE100LIB_VERSION_MAJOR 0x01
#define WGE100LIB_VERSION_MINOR 0x05
#define WGE100LIB_VERSION ((WGE100LIB_VERSION_MAJOR <<8) | WGE100LIB_VERSION_MINOR )


/*
 * Trigger Modes
 */

#define TRIG_STATE_INTERNAL 0
#define TRIG_STATE_EXTERNAL 1
#define TRIG_STATE_ALTERNATE 4
#define TRIG_STATE_RISING 2 

/*
 * The wge100FrameInfo structure is returned to the frame handler
 */  
typedef struct
{
  size_t width;
  size_t height;

  uint32_t frame_number;

  size_t lastMissingLine;
  size_t missingLines;
  int shortFrame;

  uint8_t *frameData;
  PacketEOF *eofInfo;

  struct timeval startTime;
} wge100FrameInfo;



int wge100LibVersion( void );

int wge100FindByUrl(const char *url, IpCamList *camera, unsigned wait_us, const char **errmsg);
int wge100Discover(const char *ifName, IpCamList *ipCamList, const char *ipAddress, unsigned wait_us);
int wge100Configure( IpCamList *camInfo, const char *ipAddress, unsigned wait_us);
int wge100StartVid( const IpCamList *camInfo, const uint8_t mac[6], const char *ipAddress, uint16_t port );
int wge100StopVid( const IpCamList *camInfo );
int wge100Reset( IpCamList *camInfo );
int wge100ReconfigureFPGA( IpCamList *camInfo );
int wge100GetTimer( const IpCamList *camInfo, uint64_t *time_us );
int wge100ReliableFlashRead( const IpCamList *camInfo, uint32_t address, uint8_t *pageDataOut, int *retries );
int wge100FlashRead( const IpCamList *camInfo, uint32_t address, uint8_t *pageDataOut );
int wge100ReliableFlashWrite( const IpCamList *camInfo, uint32_t address, const uint8_t *pageDataIn, int *retries );
int wge100FlashWrite( const IpCamList *camInfo, uint32_t address, const uint8_t *pageDataIn );
int wge100ReliableSensorRead( const IpCamList *camInfo, uint8_t reg, uint16_t *data, int *retries );
int wge100SensorRead( const IpCamList *camInfo, uint8_t reg, uint16_t *data );
int wge100ReliableSensorWrite( const IpCamList *camInfo, uint8_t reg, uint16_t data, int *retries );
int wge100SensorWrite( const IpCamList *camInfo, uint8_t reg, uint16_t data );
int wge100ConfigureBoard( const IpCamList *camInfo, uint32_t serial, MACAddress *mac );
int wge100TriggerControl( const IpCamList *camInfo, uint32_t triggerType );
int wge100SensorSelect( const IpCamList *camInfo, uint8_t index, uint32_t reg );
int wge100ImagerSetRes( const IpCamList *camInfo, uint16_t horizontal, uint16_t vertical );
int wge100ImagerModeSelect( const IpCamList *camInfo, uint32_t mode );

/// A FrameHandler function returns zero to continue to receive data, non-zero otherwise
typedef int (*FrameHandler)(wge100FrameInfo *frame_info, void *userData);
int wge100VidReceive( const char *ifName, uint16_t port, size_t height, size_t width, FrameHandler frameHandler, void *userData );
int wge100VidReceiveAuto( IpCamList *camera, size_t height, size_t width, FrameHandler frameHandler, void *userData );

#define CONFIG_PRODUCT_ID 6805018
#define ERR_TIMEOUT 100
#define ERR_CONFIG_ARPFAIL 200

#ifdef __cplusplus
}
#endif

#endif // _WGE100LIB_H_
