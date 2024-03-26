#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process illegal memory access is running with %d args at position %p\n", argc, argv);

  long addr = 0x1fffff;

  int *p = (int *)addr;

  *p = 1;

  return 0;
}
