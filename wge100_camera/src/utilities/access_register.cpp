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
  
int sensorread(IpCamList *camera, uint8_t reg)
{
  uint16_t val;

  if ( wge100ReliableSensorRead( camera, reg, &val, NULL ) != 0) {
    fprintf(stderr, "Could not get register.\n");
    return -1;
  }

  return val;
}

int sensorwrite(IpCamList *camera, uint8_t reg, uint16_t val)
{
  if ( wge100ReliableSensorWrite( camera, reg, val, NULL ) != 0) {
    fprintf(stderr, "Could not set register.\n");
    return -1;
  }
  return 0;
}

int usage(int argc, char **argv)
{  
  fprintf(stderr, "Usage: %s <camera_url>                                  # Read all imager register\n", argv[0]);
  fprintf(stderr, "       %s <camera_url> <register>                       # Read an imager registers\n", argv[0]);
  fprintf(stderr, "       %s <camera_url> <register> <value>               # Write an imager register\n", argv[0]);
  fprintf(stderr, "       %s <camera_url> stresstest <register>            # Repeatedly reads imager register\n", argv[0]);
  fprintf(stderr, "       %s <camera_url> stresstest <register> <value>    # Repeatedly writes value to imager register\n", argv[0]);
  fprintf(stderr, "Notes:\n");
  fprintf(stderr, "- All <register> and <value> values are in hexadecimal.\n");
  fprintf(stderr, "- This tool will never reconfigure the camera's IP address to allow its use during image streaming.\n");
  fprintf(stderr, "- Register accesses during image streaming may cause interruptions in the streaming.\n");
  return -1;
}

int main(int argc, char** argv)
{
  bool stress = false;
  bool write = false;

  char **nextarg = argv + 1;
  char **endarg = argv;
  while (*endarg)
    endarg++;

  // Get the URL
  if (nextarg == endarg || !strcmp(*nextarg, "--help")) // No arguments or --help
    return usage(argc, argv);
  const char *camera_url = *nextarg++;

  // Find the camera matching the URL
  IpCamList camera;
  const char *errmsg;
  int outval = wge100FindByUrl(camera_url, &camera, SEC_TO_USEC(0.1), &errmsg);
  if (outval)
  {
    fprintf(stderr, "Matching URL %s : %s\n", camera_url, errmsg);
    return -1;
  }

  if (nextarg == endarg) // Read all
  {
    for (int i = 0; i < 256; i++)
    {
      if (i % 16 == 0)
        fprintf(stderr, "%02x:  ", i); 
      
      if (i % 16 == 8)
        fprintf(stderr, "- "); 
        
      int value = sensorread(&camera, i);
      if (value == -1)
        fprintf(stderr, "??");
      else
        fprintf(stderr, "%04x ", value);
      
      if ((i + 1) % 16 == 0)
        fprintf(stderr, "\n"); 
    }
    return 0;
  }

  if (!strcmp(*nextarg, "stresstest")) // This is a stress test
  {
    stress = true;
    if (++nextarg == endarg)
      return usage(argc, argv);
  }
  // There is at least one argument left at this point.
  
  uint16_t val;
  uint8_t reg;
  
  sscanf(*nextarg++, "%hhx", &reg);

  if (nextarg != endarg) // It is a write.
  {
    sscanf(*nextarg++, "%hx", &val);
    write = true;
  }
                     
  if (nextarg != endarg) // Too many arguments.
    return usage(argc, argv);

  if (stress && !write)
  {
    int count = 0;
    while (1)
    {
      int oldval = sensorread(&camera, reg);
      fprintf(stderr, "Count %i reg %02x read: %04x\n", count++, reg, oldval);
    }
  }
  else if (stress)
  {
    int count = 0;
    uint16_t oldval = sensorread(&camera, reg);
    while (1)
    {
      sensorwrite(&camera, reg, val);
      int newval = sensorread(&camera, reg);
      fprintf(stderr, "Count %i Reg %02x was: %04x set: %04x read: %04x\n", count++, reg, oldval, val, newval);
      oldval = newval;
    }
  }
  else
  {
    uint16_t oldval = sensorread(&camera, reg);
    if (write)
    {
      sensorwrite(&camera, reg, val);
      int newval = sensorread(&camera, reg);
      fprintf(stderr, "Reg %02x was: %04x set: %04x read: %04x\n", reg, oldval, val, newval);
    }
    else
      fprintf(stderr, "Reg %02x read: %04x\n", reg, oldval);
  }

  return 0;
}
