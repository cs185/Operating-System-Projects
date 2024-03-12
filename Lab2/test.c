#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

// the idle process
int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);
  int i;
  for (i = 0; i < argc; i++)
  {
    TracePrintf(4, "Argument %d: %s\n", i, argv[i]);
  }
  // while (1)
  // {
  //   Pause();
  // }
  // int q = 0;
  // TracePrintf(4, "testProcess: test process is running with %d\n", q);
  int *p = (int *)malloc(1024);
  if (p == NULL)
  {
    TracePrintf(0, "testProcess: malloc failed\n");
  }
  else
  {
    TracePrintf(4, "testProcess: malloc success\n");
  }
  // int res = Fork();
  // TracePrintf(4, "testProcess: fork returns %d\n", res);
  // int h = 0;
  // while (h++ < 5)
  // {
  //   if (res == 0)
  //   {
  //     int pid = GetPid();
  //     TracePrintf(4, "testProcess: child process is running with pid %d\n", pid);
  //   }
  //   else
  //   {
  //     int pid = GetPid();
  //     TracePrintf(4, "testProcess: parent process is running with pid %d\n", pid);
  //   }

  //   Pause();
  // }
  if (Fork() == 0)
  {
    Exec("test2", argv);
  }
  else
  {
    Exec("test2", argv);
  }
  return 0;
}