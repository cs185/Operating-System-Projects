#ifndef YALNIX_PCB_H
#define YALNIX_PCB_H

#include <comp421/hardware.h>
#include <stdint.h>

#define IDLE_PROCESS 0
#define INIT_PROCESS 1

enum ListType
{
  EXECUTION_LIST,
  DELAY_LIST,
  WAIT_LIST,
  TTYREAD_LISTS,
};

// the file manages the process control block
typedef struct pcb
{
  // these are used for the exeption info to jump to the process
  void *pc;                     // program counter
  void *sp;                     // stack pointer
  unsigned long psr;            // processor status register
  unsigned long regs[NUM_REGS]; // general registers at time of exception

  // this is the real context including pc sp stc inside
  SavedContext ctx; // context of the process

  int status;      // process status
  int pid;         // process id
  int ppid;        // parent process id
  int delay;       // delay time (clock tick)
  int child_count; // the number of children of the process currently running

  uintptr_t page_table; // page table region 0 pointer (physical address)
  uintptr_t stk;        // stack page pointer, the lowest address of the last valid page of the user stack
  uintptr_t brk;        // the break of the process

  // we will use cyclic double linked list to store the process
  struct pcb *next;
  struct pcb *prev;

  int tty_read_id;
} pcb;

// create a new pcb at the end of the list
// we will leave the content of the pcb to the caller
// pid will automatically increase
struct pcb *createProcess();

// set the current process
void setCurrentProcess(struct pcb *pcb);

// get the current process
struct pcb *getCurrentProcess();

// get the next process
// if include_self is 1, we will put current process into consideration
struct pcb *getNextProcess(int include_self);

// get the next process blocked due to unable to proceed TtyRead
struct pcb *getNextTtyReadProcess(int tty_id);

// set the idle process
void setIdleProcess(struct pcb *pcb);

// get the list head of the list specified by the type
struct pcb *getList(enum ListType type);

// add the target process to the list specified by the type
void addProcessToList(struct pcb *pcb, enum ListType type);

// free the pcb, the process will be removed from the list
void removeProcessFromList(struct pcb *pcb);

// print the list of the type
void printList(enum ListType type);

// initialize the process manager
void initProcessManager();

// get the process by pid, if not found, return NULL
struct pcb *getProcessByPid(int pid);

#endif // YALNIX_PCB_H