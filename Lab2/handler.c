#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include "handler.h"
#include "pcb.h"
#include "switch.h"
#include "page.h"
#include "pte.h"
#include "load.h"

static int clock_ticks = 0;

// every process has 2 clock ticks
#define CLOCK_INTERVAL 2

// utility function to terminate the current process and get the next one running
static void terminate()
{
  struct pcb *current_process = getCurrentProcess();
  struct pcb *next_process = getNextProcess(0);
  int current_process_id = current_process->pid;
  ContextSwitch(ExitSwitch, &current_process->ctx, current_process, next_process);
  TracePrintf(2, "process %d terminated\n", current_process_id);
  TracePrintf(2, "running process %d\n", next_process->pid);
}

// the hander for system calles (trap kernel)
void onTrapKernel(ExceptionInfo *info)
{
  switch (info->code)
  {
  case YALNIX_FORK:
  {
    TracePrintf(2, "onTrapKernel: fork is called\n");
    struct pcb *current_process = getCurrentProcess();
    struct pcb *new_process = createProcess();
    new_process->ppid = current_process->pid;
    addProcessToExecutionList(new_process);
    int page_count = countPageTableEntries();
    if (getPageCount() < page_count)
    {
      TracePrintf(0, "onTrapKernel: not enough pages to allocate for fork\n");
      info->regs[0] = -1;
      break;
    }
    ContextSwitch(ForkSwitch, &current_process->ctx, current_process, new_process);
    current_process = getCurrentProcess();
    if (current_process->pid == new_process->pid)
      // if we are the child process, we will return 0
      info->regs[0] = 0;
    else
      info->regs[0] = new_process->pid;
    break;
  }
  case YALNIX_EXEC:
  {
    TracePrintf(2, "onTrapKernel: exec is called\n");

    char *filename = (char *)info->regs[1];
    char **args = (char **)info->regs[2];

    LoadProgram(filename, args);
    struct pcb *current_process = getCurrentProcess();
    info->pc = current_process->pc;
    info->sp = current_process->sp;
    // printPageTableEntries(PAGE_TABLE_0_VADDR);
    // printPagePool();

    // TracePrintf(2, "current pc is 0x%x, current sp is 0x%x\n", info->pc, info->sp);
    break;
  }
  case YALNIX_EXIT:
  {
    TracePrintf(2, "onTrapKernel: exit is called\n");

    // TODO: save the status of the current process
    struct pcb *current_process = getCurrentProcess();
    struct pcb *next_process = getNextProcess(0);

    TracePrintf(2, "onTrapKernel: exit is called, current process is %d, next process is %d\n", current_process->pid, next_process->pid);

    // we will leave the job to context switch
    // Exiting physical pages will remove the kernel stack which is dangerous outside of context switch
    ContextSwitch(ExitSwitch, &current_process->ctx, current_process, next_process);
    break;
  }
  case YALNIX_WAIT:
    TracePrintf(2, "onTrapKernel: wait is called\n");
    break;
  case YALNIX_GETPID:
  {
    TracePrintf(2, "onTrapKernel: getpid is called\n");
    struct pcb *current_process = getCurrentProcess();
    info->regs[0] = current_process->pid;
    break;
  }
  case YALNIX_BRK:
  {
    TracePrintf(2, "onTrapKernel: brk is called\n");
    // printPageTableEntries(PAGE_TABLE_0_VADDR);
    // we need to check how many pages we need to allocate
    // and then we will allocate the pages and place them into the page table
    // then we flush the TLB
    struct pcb *current_process = getCurrentProcess();
    uintptr_t next_brk = (uintptr_t)(UP_TO_PAGE((uintptr_t)info->regs[1]));
    if (next_brk < current_process->brk)
    {
      // this means we need to free some pages
      int page_count = (current_process->brk - next_brk) >> PAGESHIFT;
      TracePrintf(3, "onTrapKernel: we need to free %d pages, current break is at 0x%x, and new break is 0x%x\n", page_count, current_process->brk, next_brk);
      int i;
      for (i = 0; i < page_count; i++)
      {
        uintptr_t virtual_addr = (uintptr_t)current_process->brk - ((i + 1) << PAGESHIFT);
        removePageTableEntry(PAGE_TABLE_0_VADDR, virtual_addr, 1);
      }
    }
    else
    {
      int page_count = (next_brk - current_process->brk) >> PAGESHIFT;
      TracePrintf(3, "onTrapKernel: we need to add %d pages, current break is at 0x%x, and new break is 0x%x\n", page_count, current_process->brk, next_brk);
      uintptr_t new_pages[page_count];
      if (allocateMultiPage(page_count, new_pages) == -1)
      {
        TracePrintf(0, "onTrapKernel: failed to allocate pages\n");
        info->regs[0] = -1;
        break;
      }
      // add these pages to region 0 page table
      int i;
      for (i = 0; i < page_count; i++)
      {
        uintptr_t virtual_addr = (uintptr_t)current_process->brk + (i << PAGESHIFT);
        writePageTableEntry(PAGE_TABLE_0_VADDR, virtual_addr, new_pages[i], PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE);
      }
    }
    current_process->brk = next_brk;
    info->regs[0] = 0;
    // printPageTableEntries(PAGE_TABLE_0_VADDR);
    break;
  }
  case YALNIX_DELAY:
    TracePrintf(2, "onTrapKernel: delay is called\n");
    break;
  case YALNIX_TTY_READ:
    TracePrintf(2, "onTrapKernel: tty read is called\n");
    break;
  case YALNIX_TTY_WRITE:
    TracePrintf(2, "onTrapKernel: tty write is called\n");
    break;
  default:
    TracePrintf(0, "onTrapKernel: unknown system call is called\n");
    break;
  }
}

void onTrapClock(ExceptionInfo *info)
{
  AVOID_UNUSED_WARNING(info);
  TracePrintf(2, "onTrapClock: clock interrupt is called\n");
  TracePrintf(2, "current sp is 0x%x\n", info->sp);
  clock_ticks++;
  if (clock_ticks == CLOCK_INTERVAL)
  {
    clock_ticks = 0;
    struct pcb *current_process = getCurrentProcess();
    struct pcb *next_process = getNextProcess(1);
    ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
  }
}

void onTrapIllegal(ExceptionInfo *info)
{
  AVOID_UNUSED_WARNING(info);
  TracePrintf(2, "onTrapIllegal: illegal instruction is called\n");

  switch (info->code)
  {
  case TRAP_ILLEGAL_ILLOPC:
    TracePrintf(2, "Illegal opcode\n");
    break;
  case TRAP_ILLEGAL_ILLOPN:
    TracePrintf(2, "Illegal operand\n");
    break;
  case TRAP_ILLEGAL_ILLADR:
    TracePrintf(2, "Illegal addressing mode");
    break;
  case TRAP_ILLEGAL_ILLTRP:
    TracePrintf(2, "Illegal software trap\n");
    break;
  case TRAP_ILLEGAL_PRVOPC:
    TracePrintf(2, "Privileged opcode\n");
    break;
  case TRAP_ILLEGAL_PRVREG:
    TracePrintf(2, "Privileged register\n");
    break;
  case TRAP_ILLEGAL_COPROC:
    TracePrintf(2, "Coprocessor error\n");
    break;
  case TRAP_ILLEGAL_BADSTK:
    TracePrintf(2, "Bad stack\n");
    break;
  case TRAP_ILLEGAL_KERNELI:
    TracePrintf(2, "Linux kernel sent SIGILL\n");
    break;
  case TRAP_ILLEGAL_USERIB:
    TracePrintf(2, "Received SIGILL or SIGBUS from user\n");
    break;
  case TRAP_ILLEGAL_ADRALN:
    TracePrintf(2, "Invalid address alignment\n");
    break;
  case TRAP_ILLEGAL_ADRERR:
    TracePrintf(2, "Non-existent physical address\n");
    break;
  case TRAP_ILLEGAL_OBJERR:
    TracePrintf(2, "Object-specific HW error\n");
    break;
  case TRAP_ILLEGAL_KERNELB:
    TracePrintf(2, "Linux kernel sent SIGBUS\n");
    break;
  default:
    break;
  }

  terminate();
}

void onTrapMemory(ExceptionInfo *info)
{
  TracePrintf(2, "onTrapMemory: memory exception is called\n");
  TracePrintf(2, "current sp is 0x%x\n", info->sp);
  TracePrintf(0, "accessing memory at 0x%x\n", info->addr);
  struct pcb *current_process = getCurrentProcess();

  uintptr_t valid_stack_pointer = current_process->spp;

  switch (info->code)
  {
  case TRAP_MEMORY_MAPERR:
  {
    TracePrintf(2, "No mapping at 0x%x\n", info->addr);

    uintptr_t addr = (uintptr_t)info->addr;

    // if the address is beyond the user stack, terminate the process. Unlikely to happen
    if (addr >= KERNEL_STACK_BASE)
    {
      TracePrintf(0, "Fail to allocate new page: illegal memory\n");
      terminate();
      break;
    }

    // if the address is within allocated address of the user stack, terminate the process. Unlikely to happen
    if (addr >= valid_stack_pointer)
    {
      TracePrintf(0, "Fail to allocate new page: memory is already available\n");
      terminate();
      break;
    }

    uintptr_t lowest_addr = (uintptr_t)DOWN_TO_PAGE(addr);

    // if the actual being allocated address is in red zone, terminate the process
    if (lowest_addr < getCurrentProcess()->brk + PAGESIZE)
    {
      TracePrintf(0, "Fail to allocate new page: not enough virtual memory\n");
      terminate();
      break;
    }
    
    // compute the number of pages needed
    // which decides using allocatePage or allocateMultiPage
    int page_count = (int)((valid_stack_pointer - lowest_addr) >> PAGESHIFT);
    if (page_count == 1)
    {
      uintptr_t new_page = allocatePage();
      if (new_page == (uintptr_t)-1)
      {
        TracePrintf(0, "Fail to allocate new page: not enough physical memory\n");
        terminate();
        break;
      }
      
      TracePrintf(2, "allocated one page to the user stack\n");
      // insert the new allocated page into page table
      writePageTableEntry(PAGE_TABLE_0_VADDR, lowest_addr, new_page, PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE);
    }
    else
    {
      uintptr_t new_pages[page_count];
      if (allocateMultiPage(page_count, new_pages) == -1)
      {
        TracePrintf(0, "Fail to allocate new page: not enough physical memory\n");
        terminate();
        break;
      }

      TracePrintf(2, "allocated %d pages to the user stack\n", page_count);
      // insert the new allocated pages into page table
      int i;
      for (i = 0; i < page_count; i++)
      {
        uintptr_t virtual_addr = lowest_addr + (i << PAGESHIFT);
        writePageTableEntry(PAGE_TABLE_0_VADDR, virtual_addr, new_pages[i], PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE);
      }
    }
    
    // update the sp and spp pointer
    current_process->sp = (void*)addr;
    current_process->spp = lowest_addr;
    TracePrintf(0, "update process sp at 0x%x\n", addr);
    TracePrintf(0, "update process spp at 0x%x\n", lowest_addr);
    break;
  }
  case TRAP_MEMORY_ACCERR:
  {
    TracePrintf(2, "Protection violation at 0x%x\n", info->addr);
    terminate();
    break;
  }
  case TRAP_MEMORY_KERNEL:
  {
    TracePrintf(2, "Linux kernel sent SIGSEGV at 0x%x\n", info->addr);
    terminate();
    break;
  }
  case TRAP_MEMORY_USER:
  {
    TracePrintf(2, "Received SIGSEGV from user\n");
    terminate();
    break;
  }
  default:
  {
    TracePrintf(2, "Unknown memory trap\n");
    terminate();
    break;
  }
  }
}

void onTrapMath(ExceptionInfo *info)
{
  TracePrintf(2, "onTrapMath: math exception is called with problem of %d\n", info->code);
  // it's mostly same as calling exit
  struct pcb *current_process = getCurrentProcess();
  struct pcb *next_process = getNextProcess(0);

  // TODO write to the terminal to show the error
  // TODO save the status if needed
  ContextSwitch(ExitSwitch, &current_process->ctx, current_process, next_process);
}

void onTrapTTYReceive(ExceptionInfo *info)
{
  AVOID_UNUSED_WARNING(info);
  TracePrintf(2, "onTrapTTYReceive: tty receive is called\n");
}

void onTrapTTYTransmit(ExceptionInfo *info)
{
  AVOID_UNUSED_WARNING(info);
  TracePrintf(2, "onTrapTTYTransmit: tty transmit is called\n");
}

