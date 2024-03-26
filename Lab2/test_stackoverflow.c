#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

void test_sof()
{
  test_sof();
}

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process stack overflow is running with %d args at position %p\n", argc, argv);

  test_sof();

  return 0;
}
