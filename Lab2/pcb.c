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

static struct pcb *delay_list_head = NULL;
static struct pcb *delay_list_tail = NULL;

static struct pcb *wait_list_head = NULL;
static struct pcb *wait_list_tail = NULL;

static struct pcb **ttyread_list_heads = NULL;
static struct pcb **ttyread_list_tails = NULL;

// static struct pcb *init_process = NULL;
static struct pcb *idle_process = NULL;

#define INIT_HEAD_TAIL(head, tail)     \
  head = malloc(sizeof(struct pcb));   \
  tail = malloc(sizeof(struct pcb));   \
  memset(head, 0, sizeof(struct pcb)); \
  memset(tail, 0, sizeof(struct pcb)); \
  head->pid = -1;                      \
  tail->pid = -1;                      \
  head->tty_read_id = -1;              \
  tail->tty_read_id = -1;              \
  head->next = tail;                   \
  tail->prev = head;

void initProcessManager()
{
  INIT_HEAD_TAIL(execution_list_head, execution_list_tail);
  INIT_HEAD_TAIL(delay_list_head, delay_list_tail);
  INIT_HEAD_TAIL(wait_list_head, wait_list_tail);

  ttyread_list_heads = (struct pcb**)malloc(NUM_TERMINALS * sizeof(struct pcb*));
  ttyread_list_tails = (struct pcb**)malloc(NUM_TERMINALS * sizeof(struct pcb*));

  int i;
  for (i = 0; i < NUM_TERMINALS; i++)
  {
    INIT_HEAD_TAIL(ttyread_list_heads[i], ttyread_list_tails[i]);
  }
}

struct pcb *createProcess()
{
  struct pcb *pcb = malloc(sizeof(struct pcb));

  memset(pcb, 0, sizeof(struct pcb));
  pcb->pid = pid_counter++;
  pcb->tty_read_id = -1;
  return pcb;
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
    if (execution_list_head->next == execution_list_tail)
      // this means there is no process in the execution list
      // we need to keep on running the idle process
      return idle_process;
    else
      return execution_list_head->next;
  }
  else
  {
    if (current_process == execution_list_tail->prev)
    // last process in the list
    {
      if (current_process == execution_list_head->next)
      // only one process in the list
      {
        if (include_self)
          return current_process;
        else
          return idle_process;
      }
      else
      {
        return execution_list_head->next;
      }
    }
    else
      return current_process->next;
  }
}

struct pcb *getNextTtyReadProcess(int tty_id)
{
  if (ttyread_list_heads[tty_id]->next->pid == -1)
  // no process in the list
    return NULL;
  else
    return ttyread_list_heads[tty_id]->next;
}

struct pcb *getList(enum ListType type)
{
  switch (type)
  {
  case EXECUTION_LIST:
    return execution_list_head;
  case DELAY_LIST:
    return delay_list_head;
  case WAIT_LIST:
    return wait_list_head;
  // todo: get tty read list
  default:
    return NULL;
  }
}

void removeProcessFromList(struct pcb *pcb)
{
  if (pcb->pid < 0)
  {
    TracePrintf(0, "removeProcessFromList: trying to remove head or tail\n");
    return;
  }
  // if a process is in a list, it must have a prev and next
  // but we will check it just in case
  if (pcb->prev != NULL)
  {
    pcb->prev->next = pcb->next;
  }
  if (pcb->next != NULL)
  {
    pcb->next->prev = pcb->prev;
  }
}

void addProcessToList(struct pcb *pcb, enum ListType type)
{
  struct pcb *tail;
  switch (type)
  {
  case EXECUTION_LIST:
    tail = execution_list_tail;
    break;
  case DELAY_LIST:
    tail = delay_list_tail;
    break;
  case WAIT_LIST:
    tail = wait_list_tail;
    break;
  case TTYREAD_LISTS:
    tail = ttyread_list_tails[pcb->tty_read_id];
    break;
  default:
    TracePrintf(0, "addProcessToList: unknown type: %d\n", type);
    return;
  }

  struct pcb *last = tail->prev;
  last->next = pcb;
  pcb->prev = last;
  pcb->next = tail;
  tail->prev = pcb;
}

void printList(enum ListType type)
{
  struct pcb *current;
  switch (type)
  {
  case EXECUTION_LIST:
    TracePrintf(4, "printList: printing execution list\n");
    current = execution_list_head;
    break;
  case DELAY_LIST:
    TracePrintf(4, "printList: printing delay list\n");
    current = delay_list_head;
    break;
  case WAIT_LIST:
    TracePrintf(4, "printList: printing wait list\n");
    current = wait_list_head;
    break;
  // todo: print tty read list
  default:
    TracePrintf(0, "printList: unknown type: %d\n", type);
    return;
  }
  while (current != NULL)
  {
    TracePrintf(4, "printList: pid: %d\n", current->pid);
    current = current->next;
  }
}

struct pcb *getProcessByPid(int pid)
{
  // finding dummy process is not allowed
  if (pid < 0)
    return NULL;
  if (pid == 0)
    return idle_process;
  // iterate through all the lists
  struct pcb *current = execution_list_head;
  while (current != NULL)
  {
    if (current->pid == pid)
      return current;
    current = current->next;
  }
  current = delay_list_head;
  while (current != NULL)
  {
    if (current->pid == pid)
      return current;
    current = current->next;
  }
  current = wait_list_head;
  while (current != NULL)
  {
    if (current->pid == pid)
      return current;
    current = current->next;
  }
  return NULL;
}
