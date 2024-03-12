#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    } else {
        return fibonacci(n - 1) + fibonacci(n - 2);
    }
}

int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process fib is running with %d args at position %p\n", argc, argv);
  int i;
  for (i = 0; i < argc; i++)
  {
    TracePrintf(4, "Running fibonacci with n = %s\n", argv[i]);
    int res = fibonacci(atoi(argv[i]));
    TracePrintf(4, "Output is %d\n", res);
  }
  return 0;
}

