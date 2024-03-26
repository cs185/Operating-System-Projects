#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

// the idle process
int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  int *p = (int *)malloc(1024);
  if (p == NULL)
  {
    TracePrintf(0, "testProcess: malloc 1024 byte failed\n");
  }
  else
  {
    TracePrintf(4, "testProcess: malloc 1024 byte success\n");
  }

  int *q = (int *)malloc(0x200000);

  if (q == NULL)
  {
    TracePrintf(0, "testProcess: malloc 2 mega byte failed\n");
  }
  else
  {
    TracePrintf(4, "testProcess: malloc 2 mega byte success\n");
  }

  return 0;
}