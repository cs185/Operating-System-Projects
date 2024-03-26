#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

// the idle process
int main(int argc, char **argv)
{
  TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  int status;
  int id = Fork();
  if (id != 0)
  {
    int pid = Wait(&status);
    TracePrintf(4, "testProcess: test child process is done with pid %d and status %d\n", pid, status);
  }
  else
  {
    TracePrintf(4, "testProcess: test child process is running\n");
    Delay(5);
    // int i = 1;
    // i--;
    // int j = 1;
    // int k = j / i;
    // k++;
    TracePrintf(4, "testProcess: test child process is done\n");
    return 0;
  }
  return 0;
}