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

#include "wge100_camera/wge100lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "wge100_camera/host_netutil.h"
#include "wge100_camera/ipcam_packet.h"

/// Amount of time in microseconds that the host should wait for packet replies
#define STD_REPLY_TIMEOUT SEC_TO_USEC(0.2) // Should be tens of ms at least or flash writes will fail.
#define STOP_REPLY_TIMEOUT SEC_TO_USEC(0.001)

/**
 * Returns the version information for the library
 *
 *
 * @return  Returns the the library as an integer 0x0000MMNN, where MM = Major version and NN = Minor version
 */
int wge100libVersion() {
  return WGE100LIB_VERSION;
}


/**
 * Finds a camera matching the given name
 *
 * Names are of the form:
\verbatim
name://camera_name[@camera_ip][#local_interface]
serial://serial_number[@camera_ip][#local_interface]
any://[@camera_ip][#local_interface] 
\endverbatim
 *
 * A name URL indicates the name of the camera to be found. A serial URL
 * indicates the serial number of the camera to be found. An any URL will
 * match any camera, but if more than one camera is found, it will fail.
 *
 * Optionally, the ip address that the camera should be set to can be
 * specified by prefixing it with a @ sign, and the interface through
 * which to access the camera can be specified by prefixing it with a #
 * sign.
 *
 * When an ip address is specified, it will be copied into the camera
 * struct.
 *
 * @param url      The url of the camera to find
 * @param camera  The structure to fill the camera information into
 * @param wait_us  The number of microseconds to wait before timing out
 * @param errmsg  String containing a descriptive parse error message
 *
 * @return Returns 0 if a camera was found, -1 and sets errmsg if an error occurred.
 */

int wge100FindByUrl(const char *url, IpCamList *camera, unsigned wait_us, const char **errmsg)
{
  static const char *badprefix = "Bad URL prefix, expected name://, serial:// or any://.";
  static const char *multiaddress = "Multiple @-prefixed camera addresses found.";
  static const char *multiinterface = "Multiple #-prefixed host interfaces found.";
  static const char *discoverfailed = "Discover failed. The specified address or interface may be bad.";
  static const char *badserial = "serial:// may only be followed by an integer.";
  static const char *badany = "Unexpected characters after any://.";
  static const char *badip = "@ must be followed by a valid IP address.";
  static const char *nomatch = "No cameras matched the URL.";
  static const char *multimatch = "More than one camera matched the URL.";
  static const char *illegalcase = "Illegal case, this is a bug.";
  static const char *nomem = "malloc failed";
  const char *name = "name://"; int namelen = strlen(name);
  const char *serial = "serial://"; int seriallen = strlen(serial);
  const char *any = "any://"; int anylen = strlen(any);
  char *idstart;
  char *curpos;
  char *address = NULL;
  char *interface = NULL;
  char *copy = malloc(strlen(url) + 1);
  int retval = -1;
  int camcount = 0;
  int mode = 0;
  unsigned int serialnum = -1;
  IpCamList camList;
  IpCamList *curEntry;
  
  wge100CamListInit(&camList); // Must happen before any potential failures.

  if (errmsg)
    *errmsg = illegalcase; // If we return an error

  if (!copy) // malloc is above.
  {
    *errmsg = nomem;
    return -1;
  }
  strcpy(copy, url);

  // Decypher the prefix.
  if (!strncmp(copy, name, namelen))
  {
    idstart = copy + namelen;
    mode = 1;
  }
  else if (!strncmp(copy, serial, seriallen))
  {
    idstart = copy + seriallen;
    mode = 2;
  }
  else if (!strncmp(copy, any, anylen))
  {
    idstart = copy + anylen;
    mode = 3;
  }
  else
  {
    if (errmsg)
      *errmsg = badprefix;
    goto bailout;
  }

  // Find any # or @ words.
  for (curpos = idstart; *curpos; curpos++)
  {
    if (*curpos == '@')
    {
      if (address)
      {
        if (errmsg)
          *errmsg = multiaddress;
        goto bailout;
      }
      address = curpos + 1;
    }
    else if (*curpos == '#')
    {
      if (interface)
      {
        if (errmsg)
          *errmsg = multiinterface;
        goto bailout;
      }
      interface = curpos + 1;
    }
    else
      continue; // Didn't find anything, don't terminate the string.
    *curpos = 0;
  }
  // Now idstart, address and interface are set.

  if (mode == 3 && *idstart)
  {
    if (errmsg)
      *errmsg = badany;
    goto bailout;
  }

  if (mode == 2) // serial:// convert the number to integer.
  {
    char *endpos;
    serialnum = strtol(idstart, &endpos, 10);
    if (*idstart == 0 || *endpos != 0)
    {
      if (errmsg)
        *errmsg = badserial;
      goto bailout;
    }
  }

  // Build up a list of cameras. Only ones that are on the specified
  // interface (if specified), and that can reply from the specified
  // address (if specified) will end up in the list.
  if (wge100Discover(interface, &camList, address, wait_us) == -1)
  {
    if (errmsg)
      *errmsg = discoverfailed;
    goto bailout;
  }

  // Now search through the list for a match.
  camcount = 0;
  list_for_each_entry(curEntry, &(camList.list), list)
  {
    if ((mode == 1 && strcmp(idstart, curEntry->cam_name) == 0) ||
        (mode == 2 && serialnum == curEntry->serial) ||
        mode == 3)
    {
      camcount++;
      if (camcount > 1)
      {
        if (errmsg)
          *errmsg = multimatch;
        goto bailout;
      }
      memcpy(camera, curEntry, sizeof(IpCamList));
      if (address) // If an address has been specified, we fill it into the camera info.
      {
        struct in_addr ip;
        if (inet_aton(address, &ip))
        {
          uint8_t *ipbytes = (uint8_t *) &ip.s_addr; // Reconstruct the address, we don't want any funny stuff with the user-provided string.
          snprintf(camera->ip_str, sizeof(camera->ip_str), "%i.%i.%i.%i", ipbytes[0], ipbytes[1], ipbytes[2], ipbytes[3]);
          camera->ip = ip.s_addr;
        }
        else
        {
          if (errmsg)
            *errmsg = badip;
          goto bailout;
        }
      }
    }
  }

  switch (camcount)
  {
    case 1: // Found exactly one camera, we're good!
      retval = 0;
      break;

    case 0: // Found no cameras
      if (errmsg)
        *errmsg = nomatch;
      break;

    default:
      if (errmsg)
        *errmsg = illegalcase;
      break;
  }

bailout:
  wge100CamListDelAll(&camList);
  free(copy);
  return retval;
}

/**
 * Waits for a WGE100 StatusPacket on the specified socket for a specified duration.
 *
 * The Status type and code will be reported back to the called via the 'type' & 'code'
 * arguments. If the timeout expires before a valid status packet is received, then
 * the function will still return zero but will indicate that a TIMEOUT error has occurred.
 *
 * @param s      The open, bound  & 'connected' socket to listen on
 * @param wait_us  The number of microseconds to wait before timing out
 * @param type    Points to the location to store the type of status packet
 * @param code    Points to the location to store the subtype/error code
 *
 * @return Returns 0 if no system errors occured. -1 with errno set otherwise.
 */
int wge100StatusWait( int s, uint32_t wait_us, uint32_t *type, uint32_t *code ) {
  if( wge100WaitForPacket(&s, 1, PKTT_STATUS, sizeof(PacketStatus), &wait_us) != -1 && (wait_us != 0) ) {
    PacketStatus sPkt;
    if( recvfrom( s, &sPkt, sizeof(PacketStatus), 0, NULL, NULL )  == -1 ) {
      perror("wge100StatusWait unable to receive from socket");
      *type = PKT_STATUST_ERROR;
      *code = PKT_ERROR_SYSERR;
      return -1;
    }

    *type = ntohl(sPkt.status_type);
    *code = ntohl(sPkt.status_code);
    return 0;
  }

  *type = PKT_STATUST_ERROR;
  *code = PKT_ERROR_TIMEOUT;
  return 0;
}


static void xormem(uint8_t *dst, uint8_t *src, size_t w)
{
  while (w--)
    *dst++ ^= *src++;
}

static int wge100DiscoverSend(const char *ifName, const char *ipAddress)
{
  // Create and initialize a new Discover packet
  PacketDiscover dPkt;
  dPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  dPkt.hdr.type = htonl(PKTT_DISCOVER);
  strncpy(dPkt.hdr.hrt, "DISCOVER", sizeof(dPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(ifName, &dPkt.hdr.reply_to);
  if(s == -1) {
    //perror("Unable to create socket\n");
    return -1;
  }
  
  //// Determine the broadcast address for ifName. This is the address we
  //// will tell the camera to send from.

  struct in_addr newIP;
  if (ipAddress) // An IP was specified
  {
    inet_aton(ipAddress, &newIP);
    dPkt.ip_addr = newIP.s_addr;
  }
  else /// @todo We guess an IP by flipping the host bits of the local IP. Horrible, but won't usually be a problem even if we hit an IP that is already in use.
  {
    struct in_addr localip;
    struct in_addr netmask;
    wge100IpGetLocalAddr(ifName, &localip);
    wge100IpGetLocalNetmask(ifName, &netmask);
    dPkt.ip_addr = localip.s_addr ^ netmask.s_addr ^ ~0;
  }

  if( wge100SendUDPBcast(s, ifName, &dPkt,sizeof(dPkt)) == -1) {
    perror("Unable to send broadcast\n");
  }

  /*// For the old API
  if( wge100SendUDPBcast(s, ifName, &dPkt,sizeof(dPkt)-sizeof(dPkt.ip_addr)) == -1) {
    perror("Unable to send broadcast\n");
  }*/

  return s;
}

/**
 * Discovers all WGE100 cameras that are connected to the 'ifName' ethernet interface and
 * adds new ones to the 'ipCamList' list.
 *
 * @param ifName     The ethernet interface name to use. Null terminated string (e.g., "eth0"). Empty means to query all interfaces.
 * @param ipCamList   The list to which the new cameras should be added
 * @param wait_us    The number of microseconds to wait for replies
 *
 * @return  Returns -1 with errno set for system call errors. Otherwise returns number of new
 *       cameras found.
 */
int wge100Discover(const char *ifName, IpCamList *ipCamList, const char *ipAddress, unsigned wait_us) {
  int retval = -1;
  int *s = NULL; // Sockets to receive from
  int numif = 1;
  int nums = 0; // Number of sockets to receive from
  int i;
  const char **ifNames = NULL;
  struct ifaddrs *ifaces = NULL;
  struct ifaddrs *curif;
  int autoif = 0;

  // Count the number of new cameras found
  int newCamCount = 0;

  if (!ifName || !ifName[0]) // The interface has been specified.
  {
    autoif = 1;
    if (getifaddrs(&ifaces))
    {
      perror("getifaddrs failed");
      goto err;
    }

    numif = 0;
    for (curif = ifaces; curif; curif = curif->ifa_next)
      numif++;
    //fprintf(stderr, "There are %i interfaces.\n", numif);
  }

  ifNames = calloc(numif, sizeof(char *));
  if (!ifNames)
  {
    perror("allocating interfaces memory");
    goto err; // Okay because nums == 0 and s and ifNames are allocated or NULL.
  }

  if (!autoif)
    ifNames[0] = ifName;
  else
  {
     for (numif = 0, curif = ifaces; curif; curif = curif->ifa_next)
     {
       //fprintf(stderr, "Adding %s to discover list.\n", curif->ifa_name);
       if (curif->ifa_addr && curif->ifa_addr->sa_family == AF_INET)
         ifNames[numif++] = curif->ifa_name;
    }
  }

  s = calloc(numif, sizeof(int));
  if (!s)
  {
    perror("allocating socket memory");
    goto err; // Okay because nums == 0 and s and ifNames are allocated or NULL.
  }

  for (nums = 0; nums < numif; nums++)
  {
    s[nums] = wge100DiscoverSend(ifNames[nums], ipAddress);
    if (s[nums] == -1)
    {
      //fprintf(stderr, "Removing interface %s.\n", ifNames[nums]);
      // Delete this interface as discovery has failed on it.
      numif--;
      for (i = nums; i < numif; i++)
      {
        ifNames[i] = ifNames[i+1];
      }
      nums--;
    }
  }

  do {
    // Wait in the loop for replies. wait_us is updated each time through the loop.
    int curs = wge100WaitForPacket(s, nums, PKTT_ANNOUNCE, sizeof(PacketAnnounce) - CAMERA_NAME_LEN - sizeof(IPAddress), &wait_us);
    if( curs != -1  && wait_us != 0 ) {
      // We've received an Announce packet, so pull it out of the receive queue
      PacketAnnounce aPkt;
      struct sockaddr_in fromaddr;
      fromaddr.sin_family = AF_INET;
      socklen_t fromlen = sizeof(fromaddr);
      ssize_t packet_len;

      packet_len = recvfrom( s[curs], &aPkt, sizeof(PacketAnnounce), 0, (struct sockaddr *) &fromaddr, &fromlen);
      if(packet_len == -1 ) {
        perror("wge100Discover unable to receive from socket");
        goto err;
      }

      if (packet_len != sizeof(PacketAnnounce))
      {
        //if (packet_len != sizeof(PacketAnnounce) - sizeof(aPkt.camera_name) - sizeof(IPAddress))
          continue; // Not a valid packet
        /*else // Old announce packet
        {
          bzero(aPkt.camera_name, sizeof(aPkt.camera_name));
          aPkt.camera_ip = fromaddr.sin_addr.s_addr;
        } */
      }

      // Create a new list entry and initialize it
      IpCamList *tmpListItem;
      if( (tmpListItem = (IpCamList *)malloc(sizeof(IpCamList))) == NULL ) {
        perror("Malloc failed");
        goto err;
      }
      wge100CamListInit( tmpListItem );

      // Initialize the new list item's data fields (byte order corrected)
      tmpListItem->hw_version = ntohl(aPkt.hw_version);
      tmpListItem->fw_version = ntohl(aPkt.fw_version);
      tmpListItem->ip = aPkt.camera_ip;
      uint8_t *ipbytes = (uint8_t *) &aPkt.camera_ip;
      snprintf(tmpListItem->ip_str, sizeof(tmpListItem->ip_str), "%i.%i.%i.%i", ipbytes[0], ipbytes[1], ipbytes[2], ipbytes[3]);
      tmpListItem->serial = ntohl(aPkt.ser_no);
      memcpy(&tmpListItem->mac, aPkt.mac, sizeof(aPkt.mac));
      memcpy(tmpListItem->cam_name, aPkt.camera_name, sizeof(aPkt.camera_name));
      aPkt.camera_name[sizeof(aPkt.camera_name) - 1] = 0;
      strncpy(tmpListItem->ifName, ifNames[curs], sizeof(tmpListItem->ifName));
      tmpListItem->ifName[sizeof(tmpListItem->ifName) - 1] = 0;
      tmpListItem->status = CamStatusDiscovered;
      char pcb_rev = 0x0A + (0x0000000F & ntohl(aPkt.hw_version));
      int hdl_rev = 0x00000FFF & (ntohl(aPkt.hw_version)>>4);
      snprintf(tmpListItem->hwinfo, WGE100_CAMINFO_LEN, "PCB rev %X : HDL rev %3X : FW rev %3X", pcb_rev, hdl_rev, ntohl(aPkt.fw_version));

      // If this camera is already in the list, we don't want to add another copy
      if( wge100CamListAdd( ipCamList, tmpListItem ) == CAMLIST_ADD_DUP) {
        free(tmpListItem);
      } else {
        wge100_debug("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", aPkt.mac[0], aPkt.mac[1], aPkt.mac[2], aPkt.mac[3], aPkt.mac[4], aPkt.mac[5]);
        wge100_debug("Product #%07u : Unit #%04u\n", ntohl(aPkt.product_id), ntohl(aPkt.ser_no));
        wge100_debug("%s\n", tmpListItem->hwinfo);
        newCamCount++;
      }
    }
  } while(wait_us > 0);
  retval = newCamCount;

err:
  if (ifaces)
    freeifaddrs(ifaces);
  for (i = 0; i < nums; i++)
    close(s[i]);
  free(s);
  free(ifNames);
  return retval;
}


/**
 * Configures the IP address of one specific camera.
 *
 * @param camInfo    Structure describing the camera to configure
 * @param ipAddress  An ASCII string containing the new IP address to assign (e.g., "192.168.0.5"). If it is NULL or empty, the address in camInfo is used, and the 
 * configure packet is not broadcast.
 * @param wait_us    The number of microseconds to wait for a reply from the camera
 *
 * @return   Returns -1 with errno set for system call failures.
 *       Returns 0 for success
 *       Returns ERR_TIMEOUT if no reply is received before the timeout expires
 */
int wge100Configure( IpCamList *camInfo, const char *ipAddress, unsigned wait_us) {
  // Create and initialize a new Configure packet
  PacketConfigure cPkt;

  cPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  cPkt.hdr.type = htonl(PKTT_CONFIGURE);
  cPkt.product_id = htonl(CONFIG_PRODUCT_ID);
  strncpy(cPkt.hdr.hrt, "CONFIGURE", sizeof(cPkt.hdr.hrt));

  cPkt.ser_no = htonl(camInfo->serial);

  // Create and send the Configure broadcast packet. It is sent broadcast
  // because the camera does not yet have an IP address assigned.

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &cPkt.hdr.reply_to);
  if(s == -1) {
    perror("wge100Configure socket creation failed");
    return -1;
  }
  
  // Figure out the IP to use, and send the packet.
  if (!ipAddress || !*ipAddress)
  {
    wge100_debug("No ipAddress, using %x\n", camInfo->ip);
    cPkt.ip_addr = camInfo->ip;
    
    if(wge100SendUDP(s, &camInfo->ip, &cPkt, sizeof(cPkt)) == -1) {
      wge100_debug("Unable to send packet\n");
      close(s);
      return -1;
    }
  }
  else
  {
    struct in_addr newIP;
    inet_aton(ipAddress, &newIP);
    cPkt.ip_addr = newIP.s_addr;
    wge100_debug("Using ipAddress %s -> %x iface %s\n", ipAddress, cPkt.ip_addr, camInfo->ifName);
    
    wge100_debug("Sending broadcast discover packet.\n");
    if(wge100SendUDPBcast(s, camInfo->ifName, &cPkt, sizeof(cPkt)) == -1) {
      wge100_debug("Unable to send broadcast\n");
      close(s);
      return -1;
    }
  }
    
  // 'Connect' insures we will only receive datagram replies from the camera's new IP
  IPAddress camIP = cPkt.ip_addr;
  wge100_debug("Connecting to %x.\n", camIP);
  if( wge100SocketConnect(s, &camIP) ) {
    wge100_debug("Unable to connect\n");
    close(s);
    return -1;
  }

  // Wait up to wait_us for a valid packet to be received on s
  do {
    if( wge100WaitForPacket(&s, 1, PKTT_ANNOUNCE, sizeof(PacketAnnounce) - CAMERA_NAME_LEN - sizeof(IPAddress), &wait_us) != -1  && (wait_us != 0) ) {
      PacketAnnounce aPkt;

      if( recvfrom( s, &aPkt, sizeof(PacketAnnounce), 0, NULL, NULL )  == -1 ) {
        perror("wge100Discover unable to receive from socket");
        close(s);
        return -1;
      }

      // Need to have a valid packet from a camera with the expected serial number
      if( ntohl(aPkt.ser_no) == camInfo->serial ) {
        camInfo->status = CamStatusConfigured;
        memcpy(&camInfo->ip, &cPkt.ip_addr, sizeof(IPAddress));
        /*// Add the IP/MAC mapping to the system ARP table
        if( wge100ArpAdd(camInfo) ) {
          close(s);
          return ERR_CONFIG_ARPFAIL;
        } */ // ARP handled by the camera except for obsolete firmware versions.
        break;
      }
    }
  } while(wait_us > 0);
  close(s);

  if(wait_us != 0) {
    return 0;
  } else {
    wge100_debug("Timed out.\n");
    return ERR_TIMEOUT;
  }
}

/**
 * Instructs one camera to begin streaming video to the host/port specified.
 *
 * @param camInfo Structure that describes the camera to contact
 * @param mac    Contains the MAC address of the host that will receive the video
 * @param ipAddress An ASCII string in dotted quad form containing the IP address of the host
 *             that will receive the video (e.g., "192.168.0.5")
 * @param port    The port number that the video should be sent to. Host byte order. 
 *
 * @return   Returns -1 with errno set for system call failures
 *       Returns 0 for success
 *       Returns 1 for protocol failures (timeout, etc)
 */
int wge100StartVid( const IpCamList *camInfo, const uint8_t mac[6], const char *ipAddress, uint16_t port ) {
  PacketVidStart vPkt;

  // Initialize the packet
  vPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  vPkt.hdr.type = htonl(PKTT_VIDSTART);
  strncpy(vPkt.hdr.hrt, "Start Video", sizeof(vPkt.hdr.hrt));


  // Convert the ipAddress string into a binary value in network byte order
  inet_aton(ipAddress, (struct in_addr*)&vPkt.receiver.addr);

  // Convert the receiver port into network byte order
  vPkt.receiver.port = htons(port);

  // Copy the MAC address into the vPkt
  memcpy(&vPkt.receiver.mac, mac, 6);

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &vPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }
  
  if(wge100SendUDP(s, &camInfo->ip, &vPkt, sizeof(vPkt)) == -1) {
    goto err_out;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    goto err_out;
  }

  // Wait for a status reply
  uint32_t type, code;
  if( wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code ) == -1) {
    goto err_out;
  }

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }

err_out:
  close(s);
  return -1;
}

/**
 * Instructs one camera to stop sending video.
 *
 * @param  camInfo Describes the camera to send the stop to.
 * @return   Returns 0 for success
 *       Returns -1 with errno set for system errors
 *       Returns 1 for protocol errors
 */
int wge100StopVid( const IpCamList *camInfo ) {
  PacketVidStop vPkt;

  // Initialize the packet
  vPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  vPkt.hdr.type = htonl(PKTT_VIDSTOP);
  strncpy(vPkt.hdr.hrt, "Stop Video", sizeof(vPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &vPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &vPkt, sizeof(vPkt)) == -1 ) {
    goto err_out;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) == -1) {
    goto err_out;
  }

  uint32_t type, code;
  if(wge100StatusWait( s, STOP_REPLY_TIMEOUT, &type, &code ) == -1) {
    goto err_out;
  }

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }

err_out:
  close(s);
  return -1;
}

/**
 * Instructs one camera to reconfigure its FPGA.
 *
 * @param camInfo Describes the camera that should reconfigure its FPGA.
 * @return   Returns 0 for success
 *       Returns -1 for system errors
 */
int wge100ReconfigureFPGA( IpCamList *camInfo ) {
  PacketReconfigureFPGA gPkt;

  // Initialize the packet
  gPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  gPkt.hdr.type = htonl(PKTT_RECONFIG_FPGA);
  strncpy(gPkt.hdr.hrt, "ReconfigureFPGA", sizeof(gPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &gPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &gPkt, sizeof(gPkt)) == -1 ) {
    close(s);
    return -1;
  }

  close(s);

  // Camera is no longer configured after a reset
  camInfo->status = CamStatusDiscovered;

  // There is no reply to a reset packet

  return 0;
}

/**
 * Instructs one camera to execute a soft reset.
 *
 * @param camInfo Describes the camera to reset.
 * @return   Returns 0 for success
 *       Returns -1 for system errors
 */
int wge100Reset( IpCamList *camInfo ) {
  PacketReset gPkt;

  // Initialize the packet
  gPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  gPkt.hdr.type = htonl(PKTT_RESET);
  strncpy(gPkt.hdr.hrt, "Reset", sizeof(gPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &gPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &gPkt, sizeof(gPkt)) == -1 ) {
    close(s);
    return -1;
  }


  close(s);

  // Camera is no longer configured after a reset
  camInfo->status = CamStatusDiscovered;

  // There is no reply to a reset packet

  return 0;
}

/**
 * Gets the system time of a specified camera.
 *
 * In the camera, system time is tracked as a number of clock 'ticks' since
 * the last hard reset. This function receives the number of 'ticks' as well as
 * a 'ticks_per_sec' conversion factor to return a 64-bit time result in microseconds.
 *
 * @param camInfo Describes the camera to connect to
 * @param time_us Points to the location to store the system time in us
 *
 * @return Returns 0 for success, -1 for system error, or 1 for protocol error.
 */
int wge100GetTimer( const IpCamList *camInfo, uint64_t *time_us ) {
  PacketTimeRequest gPkt;

  // Initialize the packet
  gPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  gPkt.hdr.type = htonl(PKTT_TIMEREQ);
  strncpy(gPkt.hdr.hrt, "Time Req", sizeof(gPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &gPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &gPkt, sizeof(gPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  uint32_t wait_us = STD_REPLY_TIMEOUT;
  do {
    if( wge100WaitForPacket(&s, 1, PKTT_TIMEREPLY, sizeof(PacketTimer), &wait_us) != -1 && (wait_us != 0) ) {
      PacketTimer tPkt;
      if( recvfrom( s, &tPkt, sizeof(PacketTimer), 0, NULL, NULL )  == -1 ) {
        perror("GetTime unable to receive from socket");
        close(s);
        return -1;
      }

      *time_us = (uint64_t)ntohl(tPkt.ticks_hi) << 32;
      *time_us += ntohl(tPkt.ticks_lo);

      // Need to multiply final result by 1E6 to get us from sec
      // Try to do this to limit loss of precision without overflow
      // (We will overflow the 64-bit type with this approach
      // after the camera has been up for about 11 continuous years)
      //FIXME: Review this algorithm for possible improvements.
      *time_us *= 1000;
      *time_us /= (ntohl(tPkt.ticks_per_sec)/1000);
      //debug("Got time of %llu us since last reset\n", *time_us);
      close(s);
      return 0;
    }
  } while(wait_us > 0);

  wge100_debug("Timed out waiting for time value\n");
  close(s);
  return 1;
}

/**
 * Reads one FLASH_PAGE_SIZE byte page of the camera's onboard Atmel dataflash. Does repeated attempts 
 * if an error occurs.
 *
 * @param camInfo Describes the camera to connect to.
 * @param address Specifies the 12-bit flash page address to read (0-4095)
 * @param pageDataOut Points to at least FLASH_PAGE_SIZE bytes of storage in which to place the flash data.
 * @param retries Maximum number of retries. Decremented for each retry. NULL does 10 retries.
 *
 * @return   Returns 0 for success
 *       Returns -2 for out of retries
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 *
 */

int wge100ReliableFlashRead( const IpCamList *camInfo, uint32_t address, uint8_t *pageDataOut, int *retries ) {
  int retval = -2;

  int counter = 10;

  if (retries == NULL)
    retries = &counter;
  for (; *retries > 0; (*retries)--)
  {
    retval = wge100FlashRead( camInfo, address, pageDataOut );

    if (!retval)
      return 0;

    wge100_debug("Flash read failed.");
  }

  return retval;
}

/**
 * Reads one FLASH_PAGE_SIZE byte page of the camera's onboard Atmel dataflash.
 *
 * @param camInfo Describes the camera to connect to.
 * @param address Specifies the 12-bit flash page address to read (0-4095)
 * @param pageDataOut Points to at least FLASH_PAGE_SIZE bytes of storage in which to place the flash data.
 * @param retries Indicates the maximum allowed number of retries.
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 *
 */

int wge100FlashRead( const IpCamList *camInfo, uint32_t address, uint8_t *pageDataOut ) {
  PacketFlashRequest rPkt;

  // Initialize the packet
  rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  rPkt.hdr.type = htonl(PKTT_FLASHREAD);
  if(address > FLASH_MAX_PAGENO) {
    return 1;
  }

  // The page portion of the address is 12-bits wide, range (bit9..bit21)
  rPkt.address = htonl(address<<9);

  strncpy(rPkt.hdr.hrt, "Flash read", sizeof(rPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
    return -1;
  }


  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  uint32_t wait_us = STD_REPLY_TIMEOUT;
  do {
    if( wge100WaitForPacket(&s, 1, PKTT_FLASHDATA, sizeof(PacketFlashPayload), &wait_us) != -1 && (wait_us != 0) ) {
      PacketFlashPayload fPkt;
      if( recvfrom( s, &fPkt, sizeof(PacketFlashPayload), 0, NULL, NULL )  == -1 ) {
        perror("GetTime unable to receive from socket");
        close(s);
        return -1;
      }

      // Copy the received data into the space pointed to by pageDataOut
      memcpy(pageDataOut, fPkt.data, FLASH_PAGE_SIZE);
      close(s);
      return 0;
    }
  } while(wait_us > 0);

  wge100_debug("Timed out waiting for flash value\n");
  close(s);
  return 1;
}

/**
 * Writes one FLASH_PAGE_SIZE byte page to the camera's onboard Atmel dataflash. Repeats the write until
 * the written value matches, and reads back the written page to check that
 * it has been correctly written.
 *
 * @param camInfo Describes the camera to connect to.
 * @param address Specifies the 12-bit flash page address to write (0-4095)
 * @param pageDataOut Points to at least FLASH_PAGE_SIZE bytes of storage from which to get the flash data.
 * @param retries Maximum number of retries. Decremented for each retry. NULL does 10 retries.
 *
 * @return   Returns 0 for success
 *       Returns -2 for out of retries
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 *
 */

int wge100ReliableFlashWrite( const IpCamList *camInfo, uint32_t address, const uint8_t *pageDataIn, int *retries ) {
  uint8_t buffer[FLASH_PAGE_SIZE];
  int retval = -2;
  int counter = 10;
  int first_read = 1;

  if (retries == NULL)
    retries = &counter;

  (*retries)++; // Don't count the first read as a retry.
  goto read_first; // Don't bother writing if nothing has changed.
  
  for (; *retries > 0; (*retries)--)
  {
    //printf("Trying write.\n");
    retval = wge100FlashWrite( camInfo, address, pageDataIn );
    if (retval)
    {
      wge100_debug("Failed write, retries left: %i.", *retries);
      //printf("Failed compare write.\n");
      continue;
    }
    
    first_read = 0;
read_first:
    retval = wge100ReliableFlashRead( camInfo, address, buffer, retries );
    if (retval)
    {
      //if (!first_read)
      //  wge100_debug("Failed compare read.");
      //else
        //printf("First read failed.\n");
      //printf("Failed compare read.\n");
      continue;
    }

    if (!memcmp(buffer, pageDataIn, FLASH_PAGE_SIZE))
      return 0;
    //if (!first_read)
    //  wge100_debug("Failed compare.");
    //else
      //printf("First read mismatch.\n");
    //printf("Failed compare.\n");
    
    if (*retries == 0) // In case retries ran out during the read.
      break;
  }

  return retval;
}

/**
 * Writes one FLASH_PAGE_SIZE byte page to the camera's onboard Atmel dataflash.
 *
 * @param camInfo Describes the camera to connect to.
 * @param address Specifies the 12-bit flash page address to write (0-4095)
 * @param pageDataOut Points to at least FLASH_PAGE_SIZE bytes of storage from which to get the flash data.
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 *
 */
int wge100FlashWrite( const IpCamList *camInfo, uint32_t address, const uint8_t *pageDataIn ) {
  PacketFlashPayload rPkt;

  // Initialize the packet
  rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  rPkt.hdr.type = htonl(PKTT_FLASHWRITE);
  if(address > FLASH_MAX_PAGENO) {
    return 1;
  }

  // The page portion of the address is 12-bits wide, range (bit9..bit21)
  rPkt.address = htonl(address<<9);
  strncpy(rPkt.hdr.hrt, "Flash write", sizeof(rPkt.hdr.hrt));

  memcpy(rPkt.data, pageDataIn, FLASH_PAGE_SIZE);

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  // Wait for response - once we get an OK, we're clear to send the next page.
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

/**
 * Sets the trigger type (internal or external) for one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param triggerType Can be either TRIG_STATE_INTERNAL or TRIG_STATE_EXTERNAL
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100TriggerControl( const IpCamList *camInfo, uint32_t triggerType ) {
  PacketTrigControl tPkt;

  // Initialize the packet
  tPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  tPkt.hdr.type = htonl(PKTT_TRIGCTRL);
  tPkt.trig_state = htonl(triggerType);

  if(triggerType == TRIG_STATE_INTERNAL ) {
    strncpy(tPkt.hdr.hrt, "Int. Trigger", sizeof(tPkt.hdr.hrt));
  } else {
    strncpy(tPkt.hdr.hrt, "Ext. Trigger", sizeof(tPkt.hdr.hrt));
  }

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &tPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &tPkt, sizeof(tPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  // Wait for response
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

/**
 * Sets the permanent serial number and MAC configuration for one camera
 *
 * @warning: This command can only be sent once per camera. The changes are permanent.
 *
 * @param camInfo Describes the camera to connect to
 * @param serial Is the 32-bit unique serial number to assign (the product ID portion is already fixed)
 * @param mac Is the 48-bit unique MAC address to assign to the board
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100ConfigureBoard( const IpCamList *camInfo, uint32_t serial, MACAddress *mac ) {
  PacketSysConfig sPkt;

  // Initialize the packet
  sPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  sPkt.hdr.type = htonl(PKTT_SYSCONFIG);

  strncpy(sPkt.hdr.hrt, "System Config", sizeof(sPkt.hdr.hrt));
  memcpy(&sPkt.mac, mac, 6);
  sPkt.serial = htonl(serial);


  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &sPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &sPkt, sizeof(sPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }
  // Wait for response
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

/**
 * Writes to one image sensor I2C register on one camera. Repeats reads and
 * reads back written value until it gets a match.
 *
 * @param camInfo Describes the camera to connect to
 * @param reg The 8-bit register address to write into
 * @parm data 16-bit value to write into the register
 * @param retries Maximum number of retries. Decremented for each retry. NULL does 10 retries.
 *
 * @return   Returns 0 for success
 *       Returns -2 for out of retries
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */

int wge100ReliableSensorWrite( const IpCamList *camInfo, uint8_t reg, uint16_t data, int *retries ) {
  uint16_t readbackdata;
  int retval = -2;
  int counter = 10;

  if (retries == NULL)
    retries = &counter;

  for (; *retries > 0; (*retries)--)
  {
    retval = wge100SensorWrite( camInfo, reg, data );
    if (retval)
      continue;

    retval = wge100ReliableSensorRead( camInfo, reg, &readbackdata, retries );
    if (retval)
      continue;

    if (readbackdata == data)
      return 0;
    
    if (*retries == 0) // In case retries ran out during the read.
      retval = -2;
  }

  return retval;
}
  
/**
 * Writes to one image sensor I2C register on one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param reg The 8-bit register address to write into
 * @parm data 16-bit value to write into the register
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100SensorWrite( const IpCamList *camInfo, uint8_t reg, uint16_t data ) {
  PacketSensorData sPkt;

  // Initialize the packet
  sPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  sPkt.hdr.type = htonl(PKTT_SENSORWR);

  strncpy(sPkt.hdr.hrt, "Write I2C", sizeof(sPkt.hdr.hrt));
  sPkt.address = reg;
  sPkt.data = htons(data);

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &sPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &sPkt, sizeof(sPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  // Wait for response
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

/**
 * Reads the value of one image sensor I2C register on one camera. Retries
 * if there are any errors.
 *
 * @param camInfo Describes the camera to connect to
 * @param reg The 8-bit register address to read from
 * @parm data Pointer to 16 bits of storage to write the value to
 * @param retries Maximum number of retries. Decremented for each retry. NULL does 10 retries.
 *
 * @return   Returns 0 for success
 *       Returns -2 for out of retries
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */

int wge100ReliableSensorRead( const IpCamList *camInfo, uint8_t reg, uint16_t *data, int *retries ) {
  int retval = -2;

  int counter = 10;

  if (retries == NULL)
    retries = &counter;
  for (; *retries > 0; (*retries)--)
  {
    retval = wge100SensorRead( camInfo, reg, data );

    if (!retval)
      return 0;
  }

  return retval;
}

/**
 * Reads the value of one image sensor I2C register on one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param reg The 8-bit register address to read from
 * @parm data Pointer to 16 bits of storage to write the value to
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100SensorRead( const IpCamList *camInfo, uint8_t reg, uint16_t *data ) {
  PacketSensorRequest rPkt;

  // Initialize the packet
  rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  rPkt.hdr.type = htonl(PKTT_SENSORRD);
  rPkt.address = reg;
  strncpy(rPkt.hdr.hrt, "Read I2C", sizeof(rPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  uint32_t wait_us = STD_REPLY_TIMEOUT;
  do {
    if( wge100WaitForPacket(&s, 1, PKTT_SENSORDATA, sizeof(PacketSensorData), &wait_us) != -1 && (wait_us != 0) ) {
      PacketSensorData sPkt;
      if( recvfrom( s, &sPkt, sizeof(PacketSensorData), 0, NULL, NULL )  == -1 ) {
        perror("SensorRead unable to receive from socket");
        close(s);
        return -1;
      }

      *data = ntohs(sPkt.data);
      close(s);
      return 0;
    }
  } while(wait_us > 0);

  wge100_debug("Timed out waiting for sensor value\n");
  close(s);
  return 1;
}

/**
 * Sets the address of one of the four automatically read I2C registers on one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param index The index (0..3) of the register to set
 * @param reg The 8-bit address of the register to read at position 'index', or I2C_AUTO_REG_UNUSED.
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100SensorSelect( const IpCamList *camInfo, uint8_t index, uint32_t reg ) {
  PacketSensorSelect sPkt;

  // Initialize the packet
  sPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  sPkt.hdr.type = htonl(PKTT_SENSORSEL);

  strncpy(sPkt.hdr.hrt, "Select I2C", sizeof(sPkt.hdr.hrt));
  sPkt.index = index;
  sPkt.address = htonl(reg);

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &sPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &sPkt, sizeof(sPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  // Wait for response
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

/**
 * Selects one of the 10 pre-programmed imager modes in the specified camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param mode The mode number to select, range (0..9)
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100ImagerModeSelect( const IpCamList *camInfo, uint32_t mode ) {
  PacketImagerMode mPkt;

  // Initialize the packet
  mPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  mPkt.hdr.type = htonl(PKTT_IMGRMODE);

  mPkt.mode = htonl(mode);

  strncpy(mPkt.hdr.hrt, "Set Mode", sizeof(mPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &mPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &mPkt, sizeof(mPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  // Wait for response
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

/**
 * Provides for manually setting non-standard imager modes. Use only with extreme caution.
 *
 * @warning The imager frame size set by I2C must match the one used by the firmware, or system
 * failure could result.
 *
 * @param camInfo Describes the camera to connect to
 * @param horizontal The number of 8-bit image pixels per line of video. Maximum value 752.
 * @param vertical The number of video lines per frame.
 *
 * @return   Returns 0 for success
 *       Returns -1 for system error
 *       Returns 1 for protocol error
 */
int wge100ImagerSetRes( const IpCamList *camInfo, uint16_t horizontal, uint16_t vertical ) {
  PacketImagerSetRes rPkt;

  // Initialize the packet
  rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
  rPkt.hdr.type = htonl(PKTT_IMGRSETRES);

  rPkt.horizontal = htons(horizontal);
  rPkt.vertical = htons(vertical);

  strncpy(rPkt.hdr.hrt, "Set Res", sizeof(rPkt.hdr.hrt));

  /* Create a new socket bound to a local ephemeral port, and get our local connection
   * info so we can request a reply back to the same socket.
   */
  int s = wge100CmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
  if( s == -1 ) {
    return -1;
  }

  if(  wge100SendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
    return -1;
  }

  // 'Connect' insures we will only receive datagram replies from the camera we've specified
  if( wge100SocketConnect(s, &camInfo->ip) ) {
    close(s);
    return -1;
  }

  // Wait for response
  uint32_t type, code;
  wge100StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

  close(s);
  if(type == PKT_STATUST_OK) {
    return 0;
  } else {
    wge100_debug("Error: wge100StatusWait returned status %d, code %d\n", type, code);
    return 1;
  }
}

#define MAX_HORIZ_RESOLUTION 752
#define LINE_NUMBER_MASK 0x3FF

int wge100VidReceiveSocket( int s, size_t height, size_t width, FrameHandler frameHandler, void *userData ) {
  /*
   * The default receive buffer size on a 32-bit Linux machine is only 128kB.
   * At a burst data rate of ~ 82.6Mbit/s in the 640x480 @30fps, this buffer will fill in ~12.6ms.
   * With the default Linux scheduler settings, this leaves virtually no time for any other processes
   * to execute before the buffer overflows. Increasing the buffer reduces the chance of lost
   * packets.
   *
   * The 8MB buffer here is far larger than necessary for most applications and may be reduced with
   * some experimentation.
   *
   * Note that the Linux system command 'sysctl -w net.core.rmem_max=8388608' must be used to enable
   * receive buffers larger than the kernel default.
   */
  size_t bufsize = 16*1024*1024;  // 8MB receive buffer.

  int i;

  int return_value = 0;

  if( setsockopt(s, SOL_SOCKET,SO_RCVBUF, &bufsize, sizeof(bufsize)) == -1) {
    perror("Can't set rcvbuf option");
    close(s);
    return -1;
  }

  socklen_t bufsizesize = sizeof(bufsize);
  if( getsockopt(s, SOL_SOCKET,SO_RCVBUF, &bufsize, &bufsizesize) == 0) {
    wge100_debug("Receive buffer size is: %i (%i)\n", bufsize, bufsizesize);
  }
  else
  {
    perror("Can't read receive buffer size");
  }

  // We receive data one line at a time; lines will be reassembled into this buffer as they arrive
  uint8_t *frame_buf;
  frame_buf = malloc(sizeof(uint8_t)*width*height);
  if(frame_buf == NULL) {
    perror("Can't malloc frame buffer");
    close(s);
    return -1;
  }

  // Allocate enough storage for the header as well as 'width' bytes of video data
  PacketVideoLine *vPkt=malloc(sizeof(PacketVideoLine));
  if(vPkt == NULL) {
    perror("Can't malloc line packet buffer");
    close(s);
    return -1;
  }

  // Flag to indicate that 'frameInfo.frame_number' has not yet been set
  bool firstPacket = true;

  // Flag to indicate the current frame is complete (either EOF received or first line of next frame received)
  bool frameComplete;

  // Flag to indicate that we have set the 'frameInfo.startTime' timeval for this frame.
  bool frameStartTimeSet;

  // Holds return code from frameHandler
  int handlerReturn;

  // Counts number of lines received in current frame
  uint32_t lineCount;

  // Points to an EOF structure for passing to frameHandler;
  PacketEOF *eof = NULL;

  // Information structure to pass to the frame handler.
  wge100FrameInfo frameInfo;
      
  // Keep track of where the last packet came from (to send out pings).
  struct sockaddr_in fromaddr;

  // Has the current frame had an XOR line
  bool has_xor;

  frameInfo.width = width;
  frameInfo.height = height;

  uint8_t xorline[width];

  do {
    lineCount = 0;
    frameComplete = false;
    frameStartTimeSet = false;
    frameInfo.lastMissingLine = -1;
    frameInfo.missingLines = 0;
    frameInfo.shortFrame = false;
    frameInfo.frame_number = vPkt->header.frame_number+1;
    // Ensure the buffer is cleared out. Makes viewing short packets less confusing; missing lines will be black.
    memset(frame_buf, 0, width*height);
    has_xor = false;

    // We detect a dropped EOF by unexpectedly receiving a line from the next frame before getting the EOF from this frame.
    // After the last frame has been processed, start a fresh frame with the expected line.
    if( (eof==NULL) && (firstPacket==false) ) {
      if(vPkt->header.line_number < height ) {
        memcpy(&(frame_buf[vPkt->header.line_number*width]), vPkt->data, width);
        lineCount++;
      }
      frameInfo.frame_number &= ~0xFFFF;
      frameInfo.frame_number |= vPkt->header.frame_number;
    }

    do {
      // Wait forever for video packets to arrive; could use select() with a timeout here if a timeout is needed
      struct timeval readtimeout;
      fd_set set;
      socklen_t fromaddrlen = sizeof(struct sockaddr_in);
      fromaddr.sin_family = AF_INET;

      // Wait for either a packet to be received or for timeout
      handlerReturn = 0;
      do {
        readtimeout.tv_sec = 1;
        readtimeout.tv_usec = 0;

        FD_ZERO(&set);
        FD_SET(s, &set);

        if( select(s+1, &set, NULL, NULL, &readtimeout) == -1 ) {
          perror("wge100VidReceive select failed");
          close(s);
          return -1;
        }

        // Call the frame handler with NULL to see if we should bail out. 
        if(! FD_ISSET(s, &set) && errno != EINTR) {
          wge100_debug("Select timed out. Calling handler.");
          handlerReturn = frameHandler(NULL, userData);
          if (handlerReturn)
            break;
        }
      } while (! FD_ISSET(s, &set));

      // We timed out, and the handler returned nonzero.
      if (handlerReturn) 
        break;

      if( recvfrom( s, vPkt, sizeof(HeaderVideoLine)+width, 0, (struct sockaddr *) &fromaddr, &fromaddrlen )  == -1 ) {
        perror("wge100VidReceive unable to receive from socket");
        break;
      }

      // Convert fields to host byte order for easier processing
      vPkt->header.frame_number = ntohl(vPkt->header.frame_number);
      vPkt->header.line_number = ntohs(vPkt->header.line_number);
      vPkt->header.horiz_resolution = ntohs(vPkt->header.horiz_resolution);
      vPkt->header.vert_resolution = ntohs(vPkt->header.vert_resolution);

      //debug("frame: #%i line: %i\n", vPkt->header.frame_number, 0x3FF & vPkt->header.line_number);

      // Check to make sure the frame number information is consistent within the packet
      uint16_t temp = (vPkt->header.line_number>>10) & 0x003F;
      if (((vPkt->header.line_number & IMAGER_MAGICLINE_MASK) == 0) && (temp != (vPkt->header.frame_number & 0x003F))) {
        wge100_debug("Mismatched line/frame numbers: %02X / %02X\n", temp, (vPkt->header.frame_number & 0x003F));
      }

      // Validate that the frame is the size we expected
      if( (vPkt->header.horiz_resolution != width) || (vPkt->header.vert_resolution != height) ) {
        wge100_debug("Invalid frame size received: %u x %u, expected %u x %u\n", vPkt->header.horiz_resolution, vPkt->header.vert_resolution, width, height);
        close(s);
        return 1;
      }

      // First time through we need to initialize the number of the first frame we received
      if(firstPacket == true) {
        firstPacket = false;
        frameInfo.frame_number = vPkt->header.frame_number;
      }

      // Store the start time for the frame.
      if (!frameStartTimeSet)
      {
        gettimeofday(&frameInfo.startTime, NULL);
        frameStartTimeSet = true;
      }

      // Check for frames that ended with an error
      if( (vPkt->header.line_number == IMAGER_LINENO_ERR) || 
          (vPkt->header.line_number == IMAGER_LINENO_OVF) ) {
        wge100_debug("Video error: %04X\n", vPkt->header.line_number);

        // In the case of these special 'error' EOF packets, there has been a serious internal camera failure
        // so we will abort rather than try to process the last frame.
        return_value = vPkt->header.line_number;
        break;
      } else if (vPkt->header.line_number == IMAGER_LINENO_ABORT) {
        wge100_debug("Video aborted\n");
        break;  //don't process last frame
      } else if((vPkt->header.frame_number - frameInfo.frame_number) & 0xFFFF) { // The camera only sends 16 bits of frame number
        // If we have received a line from the next frame, we must have missed the EOF somehow
        wge100_debug("Frame #%u missing EOF, got %i lines\n", frameInfo.frame_number, lineCount);
        frameComplete = true;
        // EOF was missing
        eof = NULL;
      } else if( vPkt->header.line_number == IMAGER_LINENO_XOR ) {
        memcpy(xorline, vPkt->data, width);
        has_xor = true;
      } else if( vPkt->header.line_number == IMAGER_LINENO_EOF ) {
        // This is a 'normal' (non-error) end of frame packet (line number 0x3FF)
        frameComplete = true;

        // Handle EOF data fields
        eof = (PacketEOF *)vPkt;

        // Correct to network byte order for frameHandler
        eof->ticks_hi = ntohl(eof->ticks_hi);
        eof->ticks_lo = ntohl(eof->ticks_lo);
        eof->ticks_per_sec = ntohl(eof->ticks_per_sec);

        // Correct to network byte order for frameHandler
        eof->i2c_valid = ntohl(eof->i2c_valid);
        for(i=0; i<I2C_REGS_PER_FRAME; i++) {
          eof->i2c[i] = ntohs(eof->i2c[i]);
        }

        if(lineCount != height) {
          if ((1 == (height - lineCount)) && has_xor) {
            wge100_debug("Restoring line %i\n", frameInfo.lastMissingLine);

            // reconstruct the missing line by XORing the frame's XOR line
            // with every *other* line in the frame.
            uint8_t *repair = &frame_buf[frameInfo.lastMissingLine * width];
            memcpy(repair, xorline, width);
            unsigned int y;
            for (y = 0; y < height; y++) {
              if (y != frameInfo.lastMissingLine) {
                xormem(repair, &frame_buf[y * width], width);
              }
            }
          } else {
            // Flag packet as being short for the frameHandler
            eof->header.line_number = IMAGER_LINENO_SHORT;
            frameInfo.shortFrame = true;
            frameInfo.missingLines = height - lineCount;
          }
        }
        // Move to the next frame

      } else {
        // Remove extraneous frame information from the line number field
        vPkt->header.line_number &= LINE_NUMBER_MASK;
                                 
        if(lineCount > height)
        {
          wge100_debug("Too many lines received for frame!\n")
          break;
        }

        if( vPkt->header.line_number >= vPkt->header.vert_resolution ) {
          wge100_debug("Invalid line number received: %u (max %u)\n", vPkt->header.line_number, vPkt->header.vert_resolution);
          break;
        }
        if (lineCount + frameInfo.missingLines < vPkt->header.line_number)
        {
          int missedLines = vPkt->header.line_number - lineCount - frameInfo.missingLines; 
          frameInfo.lastMissingLine = vPkt->header.line_number - 1;
          wge100_debug("Frame #%i missed %i line(s) starting at %i src port %i\n", vPkt->header.frame_number, 
              missedLines, lineCount + frameInfo.missingLines, ntohs(fromaddr.sin_port));
          frameInfo.missingLines += missedLines;
        }
        memcpy(&(frame_buf[vPkt->header.line_number*width]), vPkt->data, width);
        lineCount++;
      }
    } while(frameComplete == false);

    if( frameComplete == true ) {
      frameInfo.frameData = frame_buf;
      frameInfo.eofInfo = eof;
      frameInfo.frame_number = frameInfo.frame_number; 
      handlerReturn = frameHandler(&frameInfo, userData);
  
      // Send a packet to the camera (this ensures that the switches along
      // the way won't time out). We could get away with doing this a lot
      // less often, but it is a relatively small cost.
      uint8_t dummy = 0xff;
      if( sendto( s, &dummy, sizeof(dummy), 0, (struct sockaddr *) &fromaddr, sizeof(fromaddr) )  == -1 ) {
        handlerReturn = -1; // Should not happen, bail out...
      }
    } else {
      // We wind up here if a serious error has resulted in us 'break'ing out of the loop.
      // We won't call frameHandler, we'll just exit directly.
      handlerReturn = -1;
    }
  } while( handlerReturn == 0 );

  close(s);
  return return_value;
}

int wge100VidReceive( const char *ifName, uint16_t port, size_t height, size_t width, FrameHandler frameHandler, void *userData ) {
  struct in_addr host_addr;
  wge100IpGetLocalAddr( ifName, &host_addr );

  if( frameHandler == NULL ) {
    wge100_debug("Invalid frame handler, aborting.\n");
    return 1;
  }

  wge100_debug("wge100VidReceive ready to receive on %s (%s:%u)\n", ifName, inet_ntoa(host_addr), port);

  int s = wge100SocketCreate( &host_addr, port );
  if( s == -1 ) {
    return -1;
  }

  return wge100VidReceiveSocket( s, height, width, frameHandler, userData);
}

int wge100VidReceiveAuto( IpCamList *camera, size_t height, size_t width, FrameHandler frameHandler, void *userData ) {
  struct sockaddr localMac;
  struct in_addr localIp;
  struct sockaddr localPort;
  socklen_t localPortLen;
  int s;
  int retval;
  int port;

  if ( wge100IpGetLocalAddr(camera->ifName, &localIp) != 0) {
    fprintf(stderr, "Unable to get local IP address for interface %s", camera->ifName);
    return -1;
  }
    
  if ( wge100EthGetLocalMac(camera->ifName, &localMac) != 0 ) {
    fprintf(stderr, "Unable to get local MAC address for interface %s", camera->ifName);
    return -1;
  }
      
  if( frameHandler == NULL ) {
    wge100_debug("Invalid frame handler, aborting.\n");
    return 1;
  }

  s = wge100SocketCreate( &localIp, 0 );
  if( s == -1 ) {
    return -1;
  }

  localPortLen = sizeof(localPort);
  if (getsockname(s, &localPort, &localPortLen) == -1)
  {
    fprintf(stderr, "Unable to get local port for socket.");
    close(s);
    return -1;
  }
  
  port = ntohs(((struct sockaddr_in *)&localPort)->sin_port);
//  fprintf(stderr, "Streaming to port %i.\n", port);

  wge100_debug("wge100VidReceiveAuto ready to receive on %s (%s:%u)\n", camera->ifName, inet_ntoa(localIp), port);

  if ( wge100StartVid( camera, (uint8_t *)&(localMac.sa_data[0]), inet_ntoa(localIp), port) != 0 ) 
  {
    wge100_debug("Could not start camera streaming.");
    close (s);
    return -1;
  }

  retval = wge100VidReceiveSocket( s, height, width, frameHandler, userData);
      
  close(s);
  wge100StopVid(camera);
  return retval;
}

