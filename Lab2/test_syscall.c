#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

// the idle process
int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  if (Fork() == 0)
  {
    Exec("malloc", argv);
  }
  else
  {
    Exec("malloc", argv);
  }

  return 0;
}