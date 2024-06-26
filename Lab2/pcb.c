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

static struct pcb *tty_read_list_head = NULL;
static struct pcb *tty_read_list_tail = NULL;

static struct pcb *tty_write_list_head = NULL;
static struct pcb *tty_write_list_tail = NULL;

// static struct pcb *init_process = NULL;
static struct pcb *idle_process = NULL;

// the number of processes except the idle process
static int process_count = 0;

#define INIT_HEAD_TAIL(head, tail)     \
  head = malloc(sizeof(struct pcb));   \
  tail = malloc(sizeof(struct pcb));   \
  memset(head, 0, sizeof(struct pcb)); \
  memset(tail, 0, sizeof(struct pcb)); \
  head->pid = -1;                      \
  tail->pid = -1;                      \
  head->tty_read_id = -1;              \
  tail->tty_read_id = -1;              \
  head->tty_write_id = -1;             \
  tail->tty_write_id = -1;             \
  head->next = tail;                   \
  tail->prev = head;

void initProcessManager()
{
  INIT_HEAD_TAIL(execution_list_head, execution_list_tail);
  INIT_HEAD_TAIL(delay_list_head, delay_list_tail);
  INIT_HEAD_TAIL(wait_list_head, wait_list_tail);
  INIT_HEAD_TAIL(tty_read_list_head, tty_read_list_tail);
  INIT_HEAD_TAIL(tty_write_list_head, tty_write_list_tail);
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

// struct pcb *getNextTtyReadProcess(int tty_id)
// {
//   if (ttyread_list_heads[tty_id]->next->pid == -1)
//     // no process in the list
//     return NULL;
//   else
//     return ttyread_list_heads[tty_id]->next;
// }

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
  case TTY_READ_LIST:
    return tty_read_list_head;
  case TTY_WRITE_LIST:
    return tty_write_list_head;
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
  process_count--;
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
  case TTY_READ_LIST:
    tail = tty_read_list_tail;
    break;
  case TTY_WRITE_LIST:
    tail = tty_write_list_tail;
    break;
  default:
    TracePrintf(0, "addProcessToList: unknown type: %d\n", type);
    return;
  }
  // once added to a list, it is a valid process
  process_count++;

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
  case TTY_READ_LIST:
    TracePrintf(4, "printList: printing tty read list\n");
    current = tty_read_list_head;
    break;
  case TTY_WRITE_LIST:
    TracePrintf(4, "printList: printing tty write list\n");
    current = tty_write_list_head;
    break;
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
  current = tty_read_list_head;
  while (current != NULL)
  {
    if (current->pid == pid)
      return current;
    current = current->next;
  }
  return NULL;
}

int countProcess()
{
  return process_count;
}