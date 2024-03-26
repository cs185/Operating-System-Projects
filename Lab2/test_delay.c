#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

// the idle process
int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);
  Fork();
  Delay(5);

  TracePrintf(4, "testProcess: waky waky\n");

  return 0;
}