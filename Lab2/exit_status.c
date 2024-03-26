#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exit_status.h"

struct ExitStatus *exit_status_head = NULL;
struct ExitStatus *exit_status_tail = NULL;

void initExitStatusList()
{
  exit_status_head = malloc(sizeof(struct ExitStatus));
  exit_status_tail = malloc(sizeof(struct ExitStatus));
  memset(exit_status_head, 0, sizeof(struct ExitStatus));
  memset(exit_status_tail, 0, sizeof(struct ExitStatus));
  exit_status_head->next = exit_status_tail;
  exit_status_tail->prev = exit_status_head;
  exit_status_head->pid = -1;
  exit_status_tail->pid = -1;
}

void addExitStatus(int pid, int ppid, int status)
{
  struct ExitStatus *new_exit_status = malloc(sizeof(struct ExitStatus));
  memset(new_exit_status, 0, sizeof(struct ExitStatus));
  new_exit_status->pid = pid;
  new_exit_status->ppid = ppid;
  new_exit_status->status = status;
  new_exit_status->next = exit_status_tail;
  new_exit_status->prev = exit_status_tail->prev;
  exit_status_tail->prev->next = new_exit_status;
  exit_status_tail->prev = new_exit_status;
}

int removeExitStatus(int ppid, int *status)
{
  struct ExitStatus *current = exit_status_head->next;
  int pid = -1;
  while (current != exit_status_tail)
  {
    if (current->ppid == ppid)
    {
      // we only collect the first matching status
      if (pid == -1)
      {
        *status = current->status;
        pid = current->pid;
      }

      // but we will remove all the matching status
      current->prev->next = current->next;
      current->next->prev = current->prev;
      free(current);
    }
    current = current->next;
  }
  return pid;
}