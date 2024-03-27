#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  int p = Fork();
  if (p != 0)
    Fork();
  
  int len = 4;
  char buf[len];
  
  while (1)
  {
    TtyRead(1, buf, len);
    TtyWrite(1, buf, len);
    
  }

  TracePrintf(4, "testProcess: waky waky\n");

  return 0;
}