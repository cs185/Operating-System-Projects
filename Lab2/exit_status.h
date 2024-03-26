#ifndef YALNIX_EXIT_STATUS_H
#define YALNIX_EXIT_STATUS_H
// this file stores information about the exit status
#include <comp421/hardware.h>
#include <comp421/loadinfo.h>

typedef struct ExitStatus
{
  int status;
  int pid;
  int ppid;
  struct ExitStatus *next;
  struct ExitStatus *prev;
} ExitStatus;

// initialize the exit status list
void initExitStatusList();

// add the parent pid and the exit status to the list
// waiting to be collected by the parent
void addExitStatus(int pid, int ppid, int status);

// remove all the exit status that belongs to this pid
// return the first exit status's pid as the result
// the status will be stored in the status pointer
// if there is no exit status for this pid, return -1
int removeExitStatus(int ppid, int *status);

void printExitStatusList();

#endif // YALNIX_EXIT_STATUS_H