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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <net/if.h>
#include <unistd.h>
#include <net/if_arp.h>

#include "wge100_camera/list.h"
#include "wge100_camera/host_netutil.h"


/**
 * Add a 'permanent' ARP to IP mapping in the system ARP table for one camera.
 * Since the cameras do not support ARP, this step is necessary so that the host
 * can find them.
 *
 * @warning Under Linux, this function requires superuser privileges (or CAP_NET_ADMIN)
 *
 * @param camInfo    An IpCamList element that describes the IP and MAC of the camera
 *
 * @return  Returns 0 for success, -1 with errno set for failure.
 */
int wge100ArpAdd(IpCamList *camInfo) {
  // Create new permanent mapping in the system ARP table
  struct arpreq arp;
  int s;

  // Create a dummy socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("wge100ArpAdd can't create socket");
    return -1;
  }

  wge100_debug("Registering ARP info for S/N %d \n", camInfo->serial);

  // Populate the arpreq structure for this mapping
  ((struct sockaddr_in*)&arp.arp_pa)->sin_family = AF_INET;
  memcpy(&((struct sockaddr_in*)&arp.arp_pa)->sin_addr, &camInfo->ip, sizeof(struct in_addr));

  // This is a 'permanent' mapping; it will not time out (but will be cleared by a reboot)
  arp.arp_flags = ATF_PERM;

  arp.arp_ha.sa_family = ARPHRD_ETHER;
  memcpy(&arp.arp_ha.sa_data, camInfo->mac, 6);

  strncpy(arp.arp_dev, camInfo->ifName, sizeof(arp.arp_dev));

  if( ioctl(s, SIOCSARP, &arp) == -1 ) {
    //perror("Warning, was unable to create ARP entry (are you root?)");
    close(s);
    return -1;
  } else {
    wge100_debug("Camera %u successfully configured\n", camInfo->serial);
  }
  return 0;
}


/**
 * Remove an to IP mapping from the system ARP table for one camera.
 * This function can be used to prevent cluttering the ARP table with unused 'permanent' mappings.
 *
 * @warning Under Linux, this function requires superuser privileges (or CAP_NET_ADMIN)
 *
 * @param camInfo    An IpCamList element that describes the IP and MAC of the camera
 *
 * @return  Returns 0 for success, -1 with errno set for failure.
 */
int wge100ArpDel(IpCamList *camInfo) {
  // Create new permanent mapping in the system ARP table
  struct arpreq arp;
  int s;

  // Create a dummy socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("wge100ArpDel can't create socket");
    return -1;
  }

  wge100_debug("Removing ARP info for S/N %d \n", camInfo->serial);

  // Populate the arpreq structure for this mapping
  ((struct sockaddr_in*)&arp.arp_pa)->sin_family = AF_INET;
  memcpy(&((struct sockaddr_in*)&arp.arp_pa)->sin_addr, &camInfo->ip, sizeof(struct in_addr));

  // No flags required for removal
  arp.arp_flags = 0;

  arp.arp_ha.sa_family = ARPHRD_ETHER;
  memcpy(&arp.arp_ha.sa_data, camInfo->mac, 6);

  strncpy(arp.arp_dev, camInfo->ifName, sizeof(arp.arp_dev));

  // Make the request to the kernel
  if( ioctl(s, SIOCDARP, &arp) == -1 ) {
    perror("Warning, was unable to remove ARP entry");
    close(s);
    return -1;
  } else {
    wge100_debug("Camera %u successfully removed from ARP table\n", camInfo->serial);
  }

  return 0;
}


/**
 * Utility function to retrieve the MAC address asssociated with
 * a specified Ethernet interface name.
 *
 * @param ifName A null-terminated string containing the name of the Ethernet address (e.g., eth0)
 * @param macAddr A sockaddr structure to contain the MAC
 *
 * @return Returns 0 if successful, -1 with errno set otherwise
 */
int wge100EthGetLocalMac(const char *ifName, struct sockaddr *macAddr) {
  struct ifreq ifr;
  int s;

  // Create a dummy socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("wge100EthGetLocalMac can't create socket");
    return -1;
  }

  // Initialize the ifreq structure with the interface name
  strncpy(ifr.ifr_name,ifName,sizeof(ifr.ifr_name)-1);
  ifr.ifr_name[sizeof(ifr.ifr_name)-1]='\0';

  // Use socket ioctl to retrieve the HW (MAC) address of this interface
  if( ioctl(s, SIOCGIFHWADDR, &ifr) == -1 ) {
    fprintf(stderr, "On interface '%s': ", ifName);
    perror("wge100EthGetLocalMac ioctl failed");
    close(s);
    return -1;
  }

  // Transfer address from ifreq struct to output pointer
  memcpy(macAddr, &ifr.ifr_addr, sizeof(struct sockaddr));

  close(s);
  return 0;
}


/**
 * Utility function to retrieve the broadcast IPv4 address asssociated with
 * a specified Ethernet interface name.
 *
 * @param ifName A null-terminated string containing the name of the Ethernet address (e.g., eth0)
 * @param macAddr A in_addr structure to contain the broadcast IP
 *
 * @return Returns 0 if successful, -1 with errno set otherwise
 */
int wge100IpGetLocalBcast(const char *ifName, struct in_addr *bcast) {
  struct ifreq ifr;
  int s;

  // Create a dummy socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("wge100IpGetLocalBcast can't create socket");
    return -1;
  }

  // Initialize the ifreq structure
  strncpy(ifr.ifr_name,ifName,sizeof(ifr.ifr_name)-1);
  ifr.ifr_name[sizeof(ifr.ifr_name)-1]='\0';

  // Use socket ioctl to get broadcast address for this interface
  if( ioctl(s,SIOCGIFBRDADDR , &ifr) == -1 ) {
    //perror("wge100IpGetLocalBcast ioctl failed");
    close(s);
    return -1;
  }

  // Requires some fancy casting because the IP portion of a sockaddr_in is stored
  // after the port (which we don't need) in the struct
  memcpy(&(bcast->s_addr), &((struct sockaddr_in *)(&ifr.ifr_broadaddr))->sin_addr, sizeof(struct in_addr));

  close(s);
  return 0;
}

/**
 * Utility function to retrieve the local IPv4 address asssociated with
 * a specified Ethernet interface name.
 *
 * @param ifName A null-terminated string containing the name of the Ethernet address (e.g., eth0)
 * @param macAddr A in_addr structure to contain the local interface IP
 *
 * @return Returns 0 if successful, -1 with errno set otherwise
 */
int wge100IpGetLocalAddr(const char *ifName, struct in_addr *addr) {
  struct ifreq ifr;
  int s;

  // Create a dummy socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("wge100IpGetLocalAddr can't create socket");
    return -1;
  }

  // Initialize the ifreq structure
  strncpy(ifr.ifr_name,ifName,sizeof(ifr.ifr_name)-1);
  ifr.ifr_name[sizeof(ifr.ifr_name)-1]='\0';

  // Use socket ioctl to get the IP address for this interface
  if( ioctl(s,SIOCGIFADDR , &ifr) == -1 ) {
    //perror("wge100IpGetLocalAddr ioctl failed");
    close(s);
    return -1;
  }

  // Requires some fancy casting because the IP portion of a sockaddr_in after the port (which we don't need) in the struct
  memcpy(&(addr->s_addr), &((struct sockaddr_in *)(&ifr.ifr_broadaddr))->sin_addr, sizeof(struct in_addr));
  close(s);

  return 0;
}


/**
 * Utility function to retrieve the local IPv4 netmask asssociated with
 * a specified Ethernet interface name.
 *
 * @param ifName A null-terminated string containing the name of the Ethernet address (e.g., eth0)
 * @param macAddr A in_addr structure to contain the local interface IP
 *
 * @return Returns 0 if successful, -1 with errno set otherwise
 */
int wge100IpGetLocalNetmask(const char *ifName, struct in_addr *addr) {
  struct ifreq ifr;
  int s;

  // Create a dummy socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("wge100IpGetLocalAddr can't create socket");
    return -1;
  }

  // Initialize the ifreq structure
  strncpy(ifr.ifr_name,ifName,sizeof(ifr.ifr_name)-1);
  ifr.ifr_name[sizeof(ifr.ifr_name)-1]='\0';

  // Use socket ioctl to get the netmask for this interface
  if( ioctl(s,SIOCGIFNETMASK , &ifr) == -1 ) {
    //perror("wge100IpGetLocalNetmask ioctl failed");
    close(s);
    return -1;
  }

  // Requires some fancy casting because the IP portion of a sockaddr_in after the port (which we don't need) in the struct
  memcpy(&(addr->s_addr), &((struct sockaddr_in *)(&ifr.ifr_broadaddr))->sin_addr, sizeof(struct in_addr));
  close(s);

  return 0;
}


/**
 * Utility function to create a UDP socket and bind it to a specified address & port.
 *
 * @param addr The host IP address to bind to.
 * @param port The host UDP port to bind to. Host byte order. Port of 0 causes bind() to assign an ephemeral port.
 *
 * @return Returns the bound socket if successful, -1 with errno set otherwise
 */
int wge100SocketCreate(const struct in_addr *addr, uint16_t port) {

  // Create a UDP socket for communicating with the network
  int s;
  if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 ) {
    perror("wge100SocketCreate can't create socket");
    return -1;
  }

  // Define the address we'll be listening on
  struct sockaddr_in si_host;
  memset( (uint8_t*)&si_host, 0, sizeof(si_host) );
  si_host.sin_family = AF_INET;                // This is an INET address
  memcpy(&si_host.sin_addr, addr, sizeof(struct in_addr));  // Bind only to this address
  si_host.sin_port = htons(port);                // If port==0,bind chooses an ephemeral source port


  // Bind the socket to the requested port
  if( bind(s, (struct sockaddr *)&si_host, sizeof(si_host)) == -1 ) {
    //perror("wge100SocketCreate unable to bind");
    close(s);
    return -1;
  }

  // Setting the broadcast flag allows us to transmit to the broadcast address
  // from this socket if we want to. Some platforms do not allow unprivileged
  // users to set this flag, but Linux does.
  int flags = 1;
  if( setsockopt(s, SOL_SOCKET,SO_BROADCAST, &flags, sizeof(flags)) == -1) {
    perror("wge100SocketCreate unable to set broadcast socket option");
    close(s);
    return -1;
  }

  return s;
}


/**
 * Utility wrapper to 'connect' a datagram socket to a specific remote host.
 * Once connected, the socket can only receive datagrams from that host.
 *
 * @param s    The open and bound local socket to connect.
 * @param ip   The remote IP address to connect to.
 *
 * @return     Returns the result from the connect() system call.
 */
int wge100SocketConnect(int s, const IPAddress *ip) {

  struct sockaddr_in camIP;

  /// @todo Not sure why this hack is needed. I'm pretty sure it used to
  // work until a few days ago (late july/early june 09). Now the
  // regression tests won't work without the hack, but real cameras work
  // just fine. Perhaps the simulated camera was being discovered on
  // another interface.
  if (*ip == 0x0100007F)
    return 0; 

  camIP.sin_family = AF_INET;
  camIP.sin_port = 0;      // Unused by connect
  camIP.sin_addr.s_addr=*ip;

  if( connect(s, (struct sockaddr *)&camIP, sizeof(camIP)) == -1 ) {
    perror("Could not connect datagram socket");
    close(s);
    return -1;
  }

  return 0;
}


/**
 * Creates and binds a new command packet socket for communicating to a camera. Will always bind to an
 * ephemeral local port number.
 *
 * @param ifName Interface name to bind to. Null terminated string (e.g., "eth0")
 * @param localHost Optional pointer to a structure to receive the local host (MAC/IP/Port) information
 * @return Returns the socket if successful, -1 otherwise
 */
int wge100CmdSocketCreate(const char *ifName, NetHost *localHost) {
  // Identify the IP address of the requested interface
  struct in_addr host_addr;
  wge100IpGetLocalAddr(ifName, &host_addr);

  // Create & bind a new listening socket, s
  // We specify a port of 0 to have bind choose an available host port
  int s;
  if( (s=wge100SocketCreate(&host_addr, 0)) == -1 ) {
    //perror("Unable to create socket");
    return -1;
  }

  if(localHost != NULL) {
    struct sockaddr_in socketAddr;
    socklen_t socketAddrSize = sizeof(socketAddr);
    if( getsockname(s, (struct sockaddr *)&socketAddr, &socketAddrSize) == -1) {
      perror("wge100SocketToNetHost Could not get socket name");
      close(s);
      return -1;
    }

    struct sockaddr macAddr;
    if( wge100EthGetLocalMac(ifName, &macAddr) == -1) {
      close(s);
      return -1;
    }

    memcpy(localHost->mac, macAddr.sa_data, sizeof(localHost->mac));
    localHost->addr = socketAddr.sin_addr.s_addr;
    localHost->port = socketAddr.sin_port;
  }

  return s;
}

/**
 * Utility function to send 'dataSize' bytes of 'data' to remote address 'ip'.
 * This function always sends to the WG_CAMCMD_PORT port.
 *
 * @param s     Bound socket to send on
 * @param ip IPv4   Address of remote camera to send to
 * @param data    Array of at least dataSize bytes, containing payload to send
 * @param dataSize  Size of payload to send, in bytes
 *
 * @return Returns 0 if successful. -1 with errno set otherwise.
 * Caller is responsible for closing the socket when finished.
 */
int wge100SendUDP(int s, const IPAddress *ip, const void *data, size_t dataSize) {
  // Create and initialize a sockaddr_in structure to hold remote port & IP
  struct sockaddr_in si_cam;
  memset( (uint8_t *)&si_cam, 0, sizeof(si_cam) );
  si_cam.sin_family = AF_INET;
  si_cam.sin_port = htons(WG_CAMCMD_PORT);
  si_cam.sin_addr.s_addr = *ip;

  // Send the datagram
  if( sendto(s, data, dataSize, 0, (struct sockaddr*)&si_cam, sizeof(si_cam)) == -1 ) {
    perror("wge100SendUDP unable to send packet");
    close(s);
    return -1;
  }
  return 0;
}

/**
 * Utility function that wraps wge100SendUDP to send a packet to the broadcast address on the
 * specified interface.
 *
 * @param s     Bound socket to send on
 * @param ifName   Name of interface socket is bound on. (Null terminated string, e.g., "eth0")
 * @param data      Array of at least dataSize bytes, containing payload to send
 * @param dataSize  Size of payload to send, in bytes
 *
 * @return Returns 0 if successful. -1 with errno set otherwise.
 * Caller is responsible for closing the socket when finished.
 */
int wge100SendUDPBcast(int s, const char *ifName, const void *data, size_t dataSize) {
  // Identify the broadcast address on the specified interface
  struct in_addr bcastIP;
  wge100IpGetLocalBcast(ifName, &bcastIP);

  // Use wge100SendUDP to send the broadcast packet
  return wge100SendUDP(s, &bcastIP.s_addr, data, dataSize);
}


/**
 * Waits for a specified amount of time for a WGE100 packet that matches the specified
 * length and type criteria.
 *
 *  On return, the wait_us argument is updated to reflect the amount of time still remaining
 *  in the original timeout. This can be useful when calling wge100WaitForPacket() in a loop.
 *
 * @param s    The datagram sockets to listen on. It must be opened, bound, and connected.
 * @param nums The number of sockets to listen on.
 * @param type  The WGE100 packet type to listen for. Packets that do not match this type will
 *         be discarded
 * @param pktLen The length of WGE100 packet to listen for. Packets that do not match this length
 *         will be discarded.
 * @param wait_us The duration of time to wait before timing out. Is adjusted upon return to
 *           reflect actual time remaning on the timeout.
 *
 * @return Returns -1 with errno set for system call failures, index of socket that is ready 
 *       otherwise. If wait_us is set to zero, then the wait has timed out.
 */
int wge100WaitForPacket( int *s, int nums, uint32_t type, size_t pktLen, uint32_t *wait_us ) {
  int i;
  // Convert wait_us argument into a struct timeval
  struct timeval timeout;
  timeout.tv_sec = *wait_us / 1000000UL;
  timeout.tv_usec = *wait_us % 1000000UL;

  // We have been asked to wait wait_us microseconds for a response; compute the time
  // at which the timeout will expire and store it into "timeout"
  struct timeval timestarted;
  struct timeval timenow;
  gettimeofday(&timestarted, NULL);
  gettimeofday(&timenow, NULL);
  timeradd( &timeout, &timestarted, &timeout );

  struct timeval looptimeout;
  fd_set set;
  while( timercmp( &timeout, &timenow, >= ) ) {
    int maxs = 0;
    // Since we could receive multiple packets, we need to decrease the timeout
    // to select() as we go. (Multiple packets should be an unlikely event, but
    // UDP provides no guarantees)
    timersub(&timeout, &timestarted, &looptimeout);

    FD_ZERO(&set);
    for (i = 0; i < nums; i++)
    {
      FD_SET(s[i], &set);
      if (s[i] > maxs)
        maxs = s[i];
    }

    // Wait for either a packet to be received or for timeout
    if( select(maxs+1, &set, NULL, NULL, &looptimeout) == -1 ) {
      perror("wge100WaitForPacket select failed");
      return -1;
    }

    for (i = 0; i < nums; i++) {
      // If we received a packet
      if( FD_ISSET(s[i], &set) ) {
        PacketGeneric gPkt;
        int r;
        // Inspect the packet in the buffer without removing it
        if( (r=recvfrom( s[i], &gPkt, sizeof(PacketGeneric), MSG_PEEK|MSG_TRUNC, NULL, NULL ))  == -1 ) {
          perror("wge100WaitForPacket unable to receive from socket");
          return -1;
        }
  
        // All valid WG command packets have magic_no == WG_MAGIC NO
        // We also know the minimum packet size we're looking for
        // So we can drop short or invalid packets at this stage
        if( ((unsigned int) r < pktLen) ||
              gPkt.magic_no != htonl(WG_MAGIC_NO) ||
              gPkt.type != htonl(type) ) {
          wge100_debug("Dropping packet with magic #%08X, type 0x%02X (looking for 0x%02X), length %d (looking for %d)\n", ntohl(gPkt.magic_no), ntohl(gPkt.type), type, r, pktLen);
          // Pull it out of the buffer (we used MSG_PEEK before, so it's still in there)
          if( recvfrom( s[i], &gPkt, sizeof(PacketGeneric), 0, NULL, NULL ) == -1 ) {
            perror("wge100WaitForPacket unable to receive from socket");
            return -1;
          }
        } else {  // The packet appears to be valid and correct
          // Compute the amount of time left on the timeout in case the calling function
          // decides this is not the packet we're looking for
          struct timeval timeleft;
          gettimeofday(&timenow, NULL);
          timersub(&timeout, &timenow, &timeleft);
  
          if (timeleft.tv_sec < 0) 
            // Otherwise we risk returning a very large number.
            *wait_us = 0;
          else
            *wait_us = timeleft.tv_usec+timeleft.tv_sec*1000000UL;              
          return i;
        }
  
      }
    }
    gettimeofday(&timenow, NULL);
  } 
  // If we reach here, we've timed out
  *wait_us = 0;
  return 0;
}
