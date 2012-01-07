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
  for (int i = 0; i < FLASH_PAGE_SIZE; i++)
    sum += htons(data[i]);
 return htons(0xFFFF - sum);
}

int read_calibration(IpCamList *camera)
{
  uint8_t calbuff[2 * FLASH_PAGE_SIZE];

  fprintf(stderr, "Reading old calibration...\n");
  if(wge100ReliableFlashRead(camera, FLASH_CALIBRATION_PAGENO, (uint8_t *) calbuff, NULL) != 0 ||
     wge100ReliableFlashRead(camera, FLASH_CALIBRATION_PAGENO+1, (uint8_t *) calbuff+FLASH_PAGE_SIZE, NULL) != 0)
  {
    fprintf(stderr, "Flash read error. Aborting.\n");
    return -2;
  }
 
  uint16_t chk = checksum((uint16_t *) calbuff);
  if (chk)
  {
    fprintf(stderr, "Previous camera calibration had bad checksum.\n");
  }                                             
  else
  {
    calbuff[sizeof(calbuff) - 2] = 0; // Make sure it is zero-terminated.
    printf("%s\n", calbuff);
  }

  return 0;
}

int write_calibration(IpCamList *camera, char *filename)
{
  uint8_t calbuff[FLASH_PAGE_SIZE * 2];
  bzero(calbuff, sizeof(calbuff));
  
  fprintf(stderr, "\nWriting new calibration...\n");
  
  FILE *f;
  if (strcmp(filename, "-"))
    f = fopen(filename, "r");
  else
  {
    fprintf(stderr, "Enter new calibration information on standard input.\n");
    f = stdin; 
  }
  if (!f)
  {
    fprintf(stderr, "Unable to open file %s.\n", filename);
    return -1;
  }
  int maxsize = sizeof(calbuff) - sizeof(uint16_t) - 1;
  int bytesread = fread(calbuff, 1, maxsize + 1, f);
  fclose(f);
  if (bytesread > maxsize)
  {
    fprintf(stderr, "File %s is too long. At most %i bytes can be stored.\n", filename, maxsize);
    return -1;
  }
  calbuff[bytesread] = 0; // Make sure we zero-terminate the data.

  ((uint16_t *) calbuff)[FLASH_PAGE_SIZE - 1] = 0;
  ((uint16_t *) calbuff)[FLASH_PAGE_SIZE - 1] = checksum((uint16_t *) calbuff);

  if (wge100ReliableFlashWrite(camera, FLASH_CALIBRATION_PAGENO, (uint8_t *) calbuff, NULL) != 0 ||
      wge100ReliableFlashWrite(camera, FLASH_CALIBRATION_PAGENO+1, (uint8_t *) calbuff+FLASH_PAGE_SIZE, NULL) != 0)
  {    
    fprintf(stderr, "Flash write error. The camera calibration is an undetermined state.\n");
    return -2;
  }
  
  fprintf(stderr, "Success!\n");
  return 0;
}

int clear_calibration(IpCamList *camera, char *filename)
{
  uint8_t calbuff[FLASH_PAGE_SIZE * 2];
  bzero(calbuff, sizeof(calbuff));
  
  fprintf(stderr, "\nClearing calibration...\n");
  
  if (wge100ReliableFlashWrite(camera, FLASH_CALIBRATION_PAGENO, (uint8_t *) calbuff, NULL) != 0 ||
      wge100ReliableFlashWrite(camera, FLASH_CALIBRATION_PAGENO+1, (uint8_t *) calbuff+FLASH_PAGE_SIZE, NULL) != 0)
  {    
    fprintf(stderr, "Flash write error. The camera calibration is an undetermined state.\n");
    return -2;
  }
  
  fprintf(stderr, "Success!\n");
  return 0;
}

int main(int argc, char **argv)
{
  if ((argc != 3 && argc != 2) || !strcmp(argv[1], "--help")) {
    fprintf(stderr, "Usage: %s <camera_url> <calibration_file>    # Sets the camera calibration information\n", argv[0]);
    fprintf(stderr, "       %s <camera_url> -                     # Sets the camera calibration information from stdin\n", argv[0]);
    fprintf(stderr, "       %s <camera_url> --invalidate          # Invalidates the camera calibration information\n", argv[0]);
    fprintf(stderr, "       %s <camera_url>                       # Reads the camera calibration information\n", argv[0]);
    fprintf(stderr, "\nReads or writes the camera calibration information stored on the camera's flash.\n");
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

  outval = read_calibration(&camera);
  if (outval)
    return outval;

  if (argc != 3)
    return 0;

  char *filename = argv[2];

  if (strcmp(filename, "--invalidate"))
    return write_calibration(&camera, filename);
  else
    return clear_calibration(&camera, filename);

}
