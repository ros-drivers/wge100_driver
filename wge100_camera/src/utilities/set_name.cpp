/*********************************************************************
 * * Software License Agreement (BSD License)
 * *
 * *  Copyright (c) 2009-2010, Willow Garage, Inc.
 * *  All rights reserved.
 * *
 * *  Redistribution and use in source and binary forms, with or without
 * *  modification, are permitted provided that the following conditions
 * *  are met:
 * *
 * *   * Redistributions of source code must retain the above copyright
 * *     notice, this list of conditions and the following disclaimer.
 * *   * Redistributions in binary form must reproduce the above
 * *     copyright notice, this list of conditions and the following
 * *     disclaimer in the documentation and/or other materials provided
 * *     with the distribution.
 * *   * Neither the name of the Willow Garage nor the names of its
 * *     contributors may be used to endorse or promote products derived
 * *     from this software without specific prior written permission.
 * *
 * *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * *  POSSIBILITY OF SUCH DAMAGE.
 * *********************************************************************/

// Author: Blaise Gassend

#include <assert.h>
#include <iostream>
#include <fstream>
 
#include <string.h> // for memset(3)
#include <stdlib.h> // for atoi(3)

#include <ros/console.h>
#include <ros/time.h>

#include <wge100_camera/ipcam_packet.h>
#include <wge100_camera/host_netutil.h>
#include <wge100_camera/wge100lib.h>
  
uint16_t checksum(uint16_t *data)
{
  uint16_t sum = 0;
  for (int i = 0; i < FLASH_PAGE_SIZE / 2; i++)
    sum += htons(data[i]);
 return htons(0xFFFF - sum);
}

int read_name(IpCamList *camera)
{
  uint8_t namebuff[FLASH_PAGE_SIZE];
  IdentityFlashPage *id = (IdentityFlashPage *) &namebuff;

  if(wge100ReliableFlashRead(camera, FLASH_NAME_PAGENO, (uint8_t *) namebuff, NULL) != 0)
  {
    fprintf(stderr, "Flash read error. Aborting.\n");
    return -2;
  }
 
  uint16_t chk = checksum((uint16_t *) namebuff);
  if (chk)
  {
    fprintf(stderr, "Previous camera name had bad checksum.\n");
  }
  
  id->cam_name[sizeof(id->cam_name) - 1] = 0;
  printf("Previous camera name was: %s\n", id->cam_name);
  uint8_t *oldip = (uint8_t *) &id->cam_addr;
  printf("Previous camera IP: %i.%i.%i.%i\n", oldip[0], oldip[1], oldip[2], oldip[3]);

  return 0;
}

int write_name(IpCamList *camera, char *name, char *new_ip)
{
  uint8_t namebuff[FLASH_PAGE_SIZE];
  IdentityFlashPage *id = (IdentityFlashPage *) &namebuff;
  
  if (strlen(name) > sizeof(id->cam_name) - 1)
  {
    fprintf(stderr, "Name is too long, the maximum number of characters is %zu.\n", sizeof(id->cam_name) - 1);
    return -2;
  }
  bzero(namebuff, FLASH_PAGE_SIZE);
  strncpy(id->cam_name, name, sizeof(id->cam_name) - 1);
  id->cam_name[sizeof(id->cam_name) - 1] = 0;
  struct in_addr cam_ip;
  if (!inet_aton(new_ip, &cam_ip))
  {
    fprintf(stderr, "This is not a valid IP address: %s\n", new_ip);
    return -2;
  }
  id->cam_addr = cam_ip.s_addr;
  id->checksum = checksum((uint16_t *) namebuff);

  if (wge100ReliableFlashWrite(camera, FLASH_NAME_PAGENO, (uint8_t *) namebuff, NULL) != 0)
  {    
    fprintf(stderr, "Flash write error. The camera name is an undetermined state.\n");
    return -2;
  }
  
  fprintf(stderr, "Success! Restarting camera, should take about 10 seconds to come back up after this.\n");

  wge100Reset(camera);

  return 0;
}

int main(int argc, char **argv)
{
  if ((argc != 4 && argc != 2) || !strcmp(argv[1], "--help")) {
    fprintf(stderr, "Usage: %s <camera_url> <new_name> <new_default_ip>   # Sets the camera name and default IP\n", argv[0]);
    fprintf(stderr, "       %s <camera_url>                               # Reads the camera name and default IP\n", argv[0]);
    fprintf(stderr, "\nReads or writes the camera name and default IP address stored on the camera's flash.\n");
    return -1;
  }

  char *camera_url = argv[1];

  // Find the camera matching the URL
  IpCamList camera;
  const char *errmsg;
  int outval = wge100FindByUrl(camera_url, &camera, SEC_TO_USEC(0.1), &errmsg);
  if (outval)
  {
    fprintf(stderr, "Matching URL %s : %s\n", camera_url, errmsg);
    return -1;
  }

  // Configure the camera with its IP address, wait up to 500ms for completion
  outval = wge100Configure(&camera, camera.ip_str, SEC_TO_USEC(0.5));
  if (outval != 0) {
    if (outval == ERR_CONFIG_ARPFAIL) {
      fprintf(stderr, "Unable to create ARP entry (are you root?), continuing anyway\n");
    } else {
      fprintf(stderr, "IP address configuration failed\n");
      return -1;
    }
  }

  outval = read_name(&camera);
  if (outval)
    return outval;

  if (argc != 4)
    return 0;

  char *name = argv[2];
  char *new_ip = argv[3];

  return write_name(&camera, name, new_ip);
}
