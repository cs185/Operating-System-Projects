#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <comp421/yalnix.h>
#include "pcb.h"
#include "pte.h"

static int pid_counter = 0;
static struct pcb *current_process = NULL;

static struct pcb *execution_list_head = NULL;
static struct pcb *execution_list_tail = NULL;
// static struct pcb *init_process = NULL;
static struct pcb *idle_process = NULL;

struct pcb *createProcess()
{
  struct pcb *pcb = malloc(sizeof(struct pcb));

  memset(pcb, 0, sizeof(struct pcb));
  pcb->pid = pid_counter++;
  return pcb;
}

void removeProcess(struct pcb *pcb, int destroy)
{
  if (pcb == execution_list_head)
  {
    execution_list_head = pcb->next;
  }
  if (pcb == execution_list_tail)
  {
    execution_list_tail = pcb->prev;
  }
  if (pcb->prev != NULL)
  {
    pcb->prev->next = pcb->next;
  }
  if (pcb->next != NULL)
  {
    pcb->next->prev = pcb->prev;
  }
  if (destroy)
  {
    free(pcb);
  }
}

void setCurrentProcess(struct pcb *pcb)
{
  current_process = pcb;
}

struct pcb *getCurrentProcess()
{
  return current_process;
}

void setIdleProcess(struct pcb *pcb)
{
  idle_process = pcb;
}

struct pcb *getNextProcess(int include_self)
{
  if (current_process == idle_process)
  {
    if (execution_list_head == NULL)
      // this means there is no process in the execution list
      // we need to keep on running the idle process
      return idle_process;
    else
      return execution_list_head;
  }
  else
  {
    if (current_process == execution_list_tail)
    {
      if (current_process == execution_list_head)
      {
        // it's the last process in the list
        if (include_self)
          return current_process;
        else
          return idle_process;
      }
      else
      {
        return execution_list_head;
      }
    }
    else
      return current_process->next;
  }
}

void addProcessToExecutionList(struct pcb *pcb)
{
  if (execution_list_head == NULL)
  {
    execution_list_head = pcb;
    execution_list_tail = pcb;
  }
  else
  {
    execution_list_tail->next = pcb;
    pcb->prev = execution_list_tail;
    execution_list_tail = pcb;
  }
}

void printExecutionList()
{
  struct pcb *current = execution_list_head;
  while (current != NULL)
  {
    TracePrintf(0, "printExecutionList: pid: %d\n", current->pid);
    current = current->next;
  }
}