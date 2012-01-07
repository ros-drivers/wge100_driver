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

#ifndef _HOST_NETUTIL_H_
#define _HOST_NETUTIL_H_

#include "build.h"
#include "list.h"
#include "ipcam_packet.h"

#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

int wge100EthGetLocalMac(const char *ifName, struct sockaddr *macAddr);
int wge100IpGetLocalNetmask(const char *ifName, struct in_addr *bcast);
int wge100IpGetLocalBcast(const char *ifName, struct in_addr *bcast);
int wge100IpGetLocalAddr(const char *ifName, struct in_addr *addr);

int wge100CmdSocketCreate(const char *ifName, NetHost *localHost);
int wge100SocketCreate(const struct in_addr *addr, uint16_t port);
int wge100SocketConnect(int s, const IPAddress *ip);
int wge100SendUDP(int s, const IPAddress *ip, const void *data, size_t dataSize);
int wge100SendUDPBcast(int s, const char *ifName, const void *data, size_t dataSize);

int wge100ArpAdd(IpCamList *camInfo);
int wge100ArpDel(IpCamList *camInfo);

int wge100WaitForPacket( int *s, int nums, uint32_t type, size_t pktLen, uint32_t *wait_us );

#define SEC_TO_USEC(sec) (1000*1000*sec)

#ifdef __cplusplus
}
#endif

#endif //_HOST_NETUTIL_H_
