#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  // int p = Fork();
  // if (p != 0)
  //   Fork();

  int len = 4;
  char buf[len];
  int i;
  for (i = 0; i < 3; i++)
  {
    TtyRead(1, buf, len);
    TtyWrite(2, buf, 10240);
  }

  i = 1;
  i--;
  int j = 1 / i;
  j++;

  TracePrintf(4, "testProcess: waky waky\n");

  return 0;
}