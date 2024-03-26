#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "page.h"
#include "pcb.h"
#include "load.h"
#include "pte.h"
#include "switch.h"
#include "handler.h"
#include "exit_status.h"
#include "terminal.h"

/**
 * rule for level of trace:
 * 0: error and debug information
 * 1: procedural information (entering function, leaving function, etc.)
 * 2: important information (function call, certain stage complete, etc.)
 * 3: trace information (variable value, etc.)
 * 4: detailed trace information (inside loop, etc.)
 * after 5: more detailed trace information (inside double loop, etc.)
 */

// the top few pages are used for the page table
#define KERNEL_HEAP_LIMIT (uintptr_t) PAGE_TABLE_HELPER_2_VADDR

// the kernel break
static uintptr_t kernel_brk;
// did we enable the virtual memory?
static int virtual_memory = 0;

extern void
KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args)
{
  if (pmem_size <= 0)
  {
    TracePrintf(0, "KernelStart: memory size is not possitive\n");
    Halt();
  }

  if (cmd_args == NULL)
  {
    TracePrintf(0, "KernelStart: no init process detected\n");
    Halt();
  }

  TracePrintf(1, "KernelStart with break at %p and total memory of %u with args %p\n", orig_brk, pmem_size, cmd_args);

  kernel_brk = (uintptr_t)orig_brk;

  // STEP 0: initialize necessary data structures

  // we put it here because we need the virutal memory to be equal to the physical memory due to hardware requirement
  // TODO: fill the interrupt vector table
  InterruptHandler *interrupt_handlers = (InterruptHandler *)malloc(TRAP_VECTOR_SIZE * sizeof(InterruptHandler));

  WriteRegister(REG_VECTOR_BASE, (RCS421RegVal)interrupt_handlers);

  // we will also initialize the exit status list here for handler to use
  initExitStatusList();
  interrupt_handlers[TRAP_KERNEL] = onTrapKernel;
  interrupt_handlers[TRAP_CLOCK] = onTrapClock;
  interrupt_handlers[TRAP_ILLEGAL] = onTrapIllegal;
  interrupt_handlers[TRAP_MEMORY] = onTrapMemory;
  interrupt_handlers[TRAP_MATH] = onTrapMath;
  interrupt_handlers[TRAP_TTY_RECEIVE] = onTrapTTYReceive;
  interrupt_handlers[TRAP_TTY_TRANSMIT] = onTrapTTYTransmit;

  TracePrintf(3, "KernelStart: interrupt vector table is initialized:)\n");

  // we need to initialize the page pool here in order to track the pages
  // when initializing the virtual memory, we have to manually mark the used pages
  initPagePool(pmem_size);
  TracePrintf(3, "KernelStart: page tracker is initialized:)\n");

  // STEP 1: initialize virtual memory

  // use some memory for the region 1 and region 0 page table
  struct pte *page_table_1 = (struct pte *)malloc(PAGE_TABLE_SIZE);
  // we will use a physcial page for the page table, by this the kernel does not need to worry about the page table
  setPageTable1(page_table_1);

  // do this after malloc, so we can include the page table itself
  int init_kernel_data_size = ((uintptr_t)kernel_brk - VMEM_1_BASE) / PAGESIZE;

  TracePrintf(3, "KernelStart: total page size of kernel text, data and heap is %d\n", init_kernel_data_size);

  TracePrintf(3, "KernelStart: page table 1 physical address is %p\n", page_table_1);
  int i;
  for (i = 0; i < init_kernel_data_size; i++)
  {
    uintptr_t addr = VMEM_1_BASE + (i << PAGESHIFT);
    // text has read and execute permission, data and heap has read and write permission
    // _etext is the end of the kernel text
    int is_text = addr < (uintptr_t)&_etext;
    unsigned int kprot = is_text ? PROT_READ | PROT_EXEC : PROT_READ | PROT_WRITE;
    writePageTableEntry(page_table_1, addr, addr, kprot, PROT_NONE);
    usePage(addr);
  }

  // struct pte *idle_page_table_0 = (struct pte *)allocatePage();
  struct pte *page_table_0 = (struct pte *)allocatePage();
  // we will always put the page table itself at the end of region 1 page table (top of kernel heap)
  writePageTableEntry(page_table_1, (uintptr_t)PAGE_TABLE_0_VADDR, (uintptr_t)page_table_0, PROT_READ | PROT_WRITE, PROT_NONE);

  TracePrintf(3, "KernelStart: page table 0 physical address is %p\n", page_table_0);

  for (i = 0; i < KERNEL_STACK_PAGES; i++)
  {
    // the stack grows from the top of the memory
    // so we need to put these pages at the end of the page table
    int addr = KERNEL_STACK_LIMIT - ((i + 1) << PAGESHIFT);
    writePageTableEntry(page_table_0, addr, addr, PROT_READ | PROT_WRITE, PROT_NONE);
    usePage(addr);
  }

  // now we can write it to the page table registers
  WriteRegister(REG_PTR1, (RCS421RegVal)page_table_1);
  WriteRegister(REG_PTR0, (RCS421RegVal)page_table_0);
  // and enable the virtual memory
  WriteRegister(REG_VM_ENABLE, 1);

  virtual_memory = 1;

  TracePrintf(2, "KernelStart: virtual memory is enabled:)\n");

  // STEP: initialize terminals
  initTerminals();

  // printPageTableEntries(page_table_1);

  // STEP 2: initialize the idle and init process
  initProcessManager();

  // first create idle process to have pid 0
  struct pcb *idle_process = createProcess();
  // then create init process to have pid 1
  struct pcb *init_process = createProcess();

  setIdleProcess(idle_process);
  setCurrentProcess(idle_process);

  // save current context to the idle process
  idle_process->page_table = (uintptr_t)page_table_0;
  TracePrintf(3, "KernelStart: idle process page table is %p, pid is %d\n", idle_process->page_table, idle_process->pid);

  // we first load the idle process
  char *idle_argv[] = {NULL};
  LoadProgram("idle", idle_argv);
  // printPageTableEntries(PAGE_TABLE_0_VADDR);

  init_process->page_table = allocatePage();
  TracePrintf(3, "KernelStart: idle process page table is %p, pid is %d\n", idle_process->page_table, idle_process->pid);
  TracePrintf(3, "KernelStart: int process page table is %p, pid is %d\n", init_process->page_table, init_process->pid);
  // then we context switch to the init process and load the init process
  ContextSwitch(ForkSwitch, &idle_process->ctx, idle_process, init_process);

  // because idle process's context is save to here
  // we need to be causeful about which process we are in
  // we will use pid to do the check
  if (getCurrentProcess()->pid == INIT_PROCESS)
  {
    // this means we are inside of init process
    // we will load the init process
    char *init_process_name = cmd_args[0];
    char **init_argv = cmd_args + 1;
    LoadProgram(init_process_name, init_argv);

    // printPageTableEntries(PAGE_TABLE_0_VADDR);

    info->sp = init_process->sp;
    info->pc = init_process->pc;

    addProcessToList(init_process, EXECUTION_LIST);

    // use this context switch to test switch back to idle
    // ContextSwitch(NormalSwitch, &init_process->ctx, init_process, idle_process);
    TracePrintf(1, "KernelStart: kernel start finished:)\n");
  }
  else
  {
    // this means we are inside of idle process
    // because we've already loaded the idle process, we don't need to do anything
    info->sp = idle_process->sp;
    info->pc = idle_process->pc;
  }
}

extern int SetKernelBrk(void *addr)
{
  uintptr_t next_brk = (uintptr_t)UP_TO_PAGE(addr);
  TracePrintf(2, "SetKernelBrk: kernel break is set to %p with virtual mode of %d, and current kernel break is at %p\n", addr, virtual_memory, kernel_brk);
  if (next_brk <= kernel_brk)
  {
    TracePrintf(0, "SetKernelBrk: kernel break is not increased\n");
    return -1;
  }
  if (next_brk >= KERNEL_HEAP_LIMIT)
  {
    TracePrintf(0, "SetKernelBrk: kernel break is out of range\n");
    return -1;
  }
  if (virtual_memory == 0)
  {

    // when using physical memory, we don't need to do anything
    // kernel start will take care the pages for us
    kernel_brk = next_brk;
    return 0;
  }
  else
  {
    // we will first calculate the number of pages we need to add
    int page_count = (next_brk - kernel_brk) >> PAGESHIFT;
    TracePrintf(3, "SetKernelBrk: we need to add %d pages\n", page_count);
    // if we failed in the middle, we will free all the pages we have added
    uintptr_t new_pages[page_count];
    if (allocateMultiPage(page_count, new_pages) == -1)
    {
      TracePrintf(0, "SetKernelBrk: failed to allocate pages\n");
      return -1;
    }
    // add these pages to region 1 page table
    int i;
    for (i = 0; i < page_count; i++)
    {
      uintptr_t virtual_addr = kernel_brk + (i << PAGESHIFT);
      writePageTableEntry(getPageTable1(), virtual_addr, new_pages[i], PROT_READ | PROT_WRITE, PROT_NONE);
    }
    // printPageTableEntries(page_table_1);
    // update the kernel_brk
    kernel_brk = next_brk;

    return 0;
  }
}
