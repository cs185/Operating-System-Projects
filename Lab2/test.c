#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdlib.h>

// the idle process
int main(int argc, char **argv)
{
  argc++;
  argv++;
  Fork();
  Fork();
  TracePrintf(0, "I'm done!!!\n");
  // TracePrintf(4, "testProcess: test process is running with %d args at position %p\n", argc, argv);

  // int *status = (int *)0x1000;
  // int id = Fork();
  // if (id != 0)
  // {
  //   int pid = Wait(status);
  //   // TracePrintf(4, "testProcess: test child process is done with pid %d and status %d\n", pid, status);
  //   TracePrintf(4, "testProcess: test child process is done with pid %d\n", pid);
  // }
  // else
  // {
  //   Delay(5);
  //   // int i = 1;
  //   // i--;
  //   // int j = 1;
  //   // int k = j / i;
  //   // k++;
  // }
  // create a invalid address for this user
  // char *addr = (char *)0x20000;
  // char *addr = "test_wait";
  // TracePrintf(4, "executing test_wait\n");
  // Exec(addr, argv);
  // TracePrintf(0, "this shall not happen\n");
  return 0;
}