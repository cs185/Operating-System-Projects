#ifndef YALNIX_PCB_H
#define YALNIX_PCB_H

#include <comp421/hardware.h>
#include <stdint.h>

#define IDLE_PROCESS 0
#define INIT_PROCESS 1

// the file manages the process control block
typedef struct pcb
{
  void *pc;                     // program counter
  void *sp;                     // stack pointer
  unsigned long psr;            // processor status register
  unsigned long regs[NUM_REGS]; // general registers at time of exception

  int status; // process status
  int pid;    // process id
  int ppid;   // parent process id

  uintptr_t page_table; // page table region 0 pointer (physical address)
  SavedContext ctx;     // context of the process
  uintptr_t spp;        // stack page pointer, the lowest address of the last valid page of the user stack
  uintptr_t brk;        // the break of the process
  // we will use cyclic double linked list to store the process
  struct pcb *next;
  struct pcb *prev;
} pcb;

// create a new pcb at the end of the list
// we will leave the content of the pcb to the caller
// pid will automatically increase
struct pcb *createProcess();

// free the pcb, the process will be removed from any list
// pass in destroy = 1 to free the pcb fro memory
void removeProcess(struct pcb *pcb, int destroy);

// set the current process
void setCurrentProcess(struct pcb *pcb);

// get the current process
struct pcb *getCurrentProcess();

// get the next process
// if include_self is 1, we will put current process into consideration
struct pcb *getNextProcess(int include_self);

// set the idle process
void setIdleProcess(struct pcb *pcb);

// add the target process to the execution list
void addProcessToExecutionList(struct pcb *pcb);

// print the execution list
void printExecutionList();

#endif // YALNIX_PCB_H