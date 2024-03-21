#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

void test_sof(int n) {
    n++;
    test_sof(n);
    return;
}

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process sof (stack over flow) is running with %d args at position %p\n", argc, argv);
  
  test_sof(atoi(argv[0]));
  
  return 0;
}

