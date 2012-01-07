#include "wge100_camera/wge100lib.h"
#include "wge100_camera/host_netutil.h"
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

int main(int argc, char** argv)
{
  if (argc != 2 || !strcmp(argv[1], "--help")) {
    fprintf(stderr, "Usage: %s <camera URL>\n", argv[0]);
    fprintf(stderr, "Repeatedly tries to discover the indicated camera to test robustness of discovery.\n");
    return 0;
  }
  char* camera_url = argv[1];

  // Find the camera matching the URL
  IpCamList camera;
  const char *errmsg;
  while (1)
  {
    wge100FindByUrl(camera_url, &camera, SEC_TO_USEC(0.001), &errmsg);
    printf(".");
    fflush(stdout);
  }

  return 0;
}
