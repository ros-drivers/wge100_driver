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

#include "wge100_camera/wge100lib.h"
#include "wge100_camera/host_netutil.h"
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <unistd.h>

int main(int argc, char** argv)
{
  if (argc != 2 || !strcmp(argv[1], "--help")) {
    fprintf(stderr, "Usage: %s <camera_url>\n", argv[0]);
    fprintf(stderr, "Writes a test sequence to unused portions of the flash to check for\n");
    fprintf(stderr, "proper operation. It is very improbable that you need this tool in\n");
    fprintf(stderr, "normal use of the camera.\n");
    return -1;
  }
  char* camera_url = argv[1];

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
  int retval = wge100Configure(&camera, camera.ip_str, SEC_TO_USEC(0.5));
  if (retval != 0) {
    if (retval == ERR_CONFIG_ARPFAIL) {
      fprintf(stderr, "Unable to create ARP entry (are you root?), continuing anyway\n");
    } else {
      fprintf(stderr, "IP address configuration failed\n");
      return -1;
    }
  }

#define FLASH_SIZE_LONG ((FLASH_PAGE_SIZE + 3) / 4)
  uint32_t buffer[FLASH_SIZE_LONG];
  uint32_t buffer2[FLASH_PAGE_SIZE];

  fprintf(stderr, "\n*************************************************************************\n");
  fprintf(stderr, "Warning, this will overwrite potentially important flash memory contents.\n");
  fprintf(stderr, "You have 5 seconds to press CTRL+C to abort this operation.\n");
  fprintf(stderr, "*************************************************************************\n");

  sleep(5);
  
  uint32_t incr = 282475249;
  uint32_t pattern = 0;
  for (int i = FLASH_MAX_PAGENO / 2; i <= FLASH_MAX_PAGENO - 10; i++)
  {
    if (i % 100 == 0)
    {
      fprintf(stderr, ".");
      fflush(stderr);
    }
    for (int j = 0; j < FLASH_SIZE_LONG; j++)
      buffer[j] = pattern += incr;

    if (wge100ReliableFlashWrite(&camera, i, (uint8_t*) buffer, NULL) != 0) {
      fprintf(stderr, "\nFlash write error\n");
      return -1;
    }
  }
  printf("\n");
  
  int errcount = 0;
  pattern = 0;
  for (int i = FLASH_MAX_PAGENO / 2; i <= FLASH_MAX_PAGENO - 10; i++)
  {
    if (i % 100 == 0)
    {
      fprintf(stderr, ".");
      fflush(stderr);
    }
    for (int j = 0; j < FLASH_SIZE_LONG; j++)
      buffer[j] = pattern += incr;

    if (wge100ReliableFlashRead(&camera, i, (uint8_t*) buffer2, NULL) != 0) {
      fprintf(stderr, "\nFlash read error\n");
      return -1;
    }
  
    if (memcmp(buffer, buffer2, FLASH_PAGE_SIZE))
    {
      errcount++;
      fprintf(stderr, "Mismatch in page %i\n.", i);
    }
  }

  fprintf(stderr, "\nCheck completed, %i errors.\n", errcount);

  return 0;
}
