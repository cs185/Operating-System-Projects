#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  char buf1[2049];

  int i;
  for (i = 0; i < 1024; i++)
    buf1[i] = (i % 26) + 'a';

  for (i = 1024; i < 2048; i++)
    buf1[i] = (i % 10) + '0';

  buf1[2048] = '!';

  // Fork();

  // while (1)
  // {
  //   TtyWrite(1, buf1, strlen(buf1));
  // }
  TtyWrite(1, buf1, 2049);

  TracePrintf(4, "testProcess: waky waky\n");

  return 0;
}