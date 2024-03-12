

#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>

// the idle process
int main()
{
  TracePrintf(4, "idleProcess: idle process is running\n");
  while (1)
  {
    Pause();
  }
  return 0;
}