#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  int len = 100;
  char *buf2 = (char *)malloc(len);
  int i;
  for (i = 0; i < len; i++)
  {
    buf2[i] = i % 10 + '0';
  }

  TtyWrite(1, buf2, len);

  TracePrintf(4, "testProcess: waky waky\n");

  return 0;
}