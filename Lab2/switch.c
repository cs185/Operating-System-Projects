#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdlib.h>
#include "switch.h"
#include "pcb.h"
#include "pte.h"

SavedContext *NormalSwitch(SavedContext *ctxp, void *p1, void *p2)
{
  AVOID_UNUSED_WARNING(ctxp);
  struct pcb *current_process = (pcb *)p1;
  struct pcb *next_process = (pcb *)p2;

  TracePrintf(2, "NormalSwitch: switch function is called for %d and %d\n", current_process->pid, next_process->pid);
  // TracePrintf(3, "content of the current process's context: %s\n", current_process->ctx.s);
  // TracePrintf(3, "content of the next process's context: %s\n", next_process->ctx.s);

  // copy the page table of the idle process to the init process
  setCurrentProcess(next_process);

  uintptr_t page_table = next_process->page_table;

  WriteRegister(REG_PTR0, (RCS421RegVal)page_table);
  // TODO: we also need to refresh the clock interrupt
  // put the new page table onto the virtual memory
  writePageTableEntry(getPageTable1(), (uintptr_t)PAGE_TABLE_0_VADDR, page_table, PROT_READ | PROT_WRITE, PROT_NONE);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  return &next_process->ctx;
}

SavedContext *ForkSwitch(SavedContext *ctxp, void *p1, void *p2)
{
  struct pcb *current_process = (pcb *)p1;
  struct pcb *next_process = (pcb *)p2;

  TracePrintf(2, "ForkSwitch: switch function is called for %d and %d\n", current_process->pid, next_process->pid);

  uintptr_t page_table = next_process->page_table;

  // TracePrintf(2, "ForkSwitch: next page table is 0x%x, while current page table is 0x%x\n", page_table, current_process->page_table);

  // copy the page table of the idle process to the init process
  // TODO: ask TA why will this fail outside of ContextSwitch
  if (copyPageTableEntries(page_table) == -1)
  {
    // the check shall be done before the switch
    // if we failed to copy the page table entries
    // we simply continue the current process
    TracePrintf(0, "ForkSwitch: failed to copy page table entries\n");
    removeProcessFromList(next_process);
    free(next_process);
    return ctxp;
  }

  // only if we successfully copied the page table entries
  // we will change the current process
  // we can check the current process to see if we failed
  setCurrentProcess(next_process);

  WriteRegister(REG_PTR0, (RCS421RegVal)page_table);
  writePageTableEntry(getPageTable1(), (uintptr_t)PAGE_TABLE_0_VADDR, page_table, PROT_READ | PROT_WRITE, PROT_NONE);
  // we don't need to flush all the kernel pages, ony the page table of region 0 need to be refreshed
  WriteRegister(REG_TLB_FLUSH, (uintptr_t)PAGE_TABLE_0_VADDR);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  // just continue, but in the new process
  return ctxp;
}

SavedContext *ExitSwitch(SavedContext *ctxp, void *p1, void *p2)
{
  AVOID_UNUSED_WARNING(ctxp);
  struct pcb *current_process = (pcb *)p1;
  struct pcb *next_process = (pcb *)p2;

  TracePrintf(2, "ExitSwitch: switch function is called for %d and %d\n", current_process->pid, next_process->pid);
  setCurrentProcess(next_process);
  // we can free the current process
  TracePrintf(2, "ExitSwitch: free the current process %d\n", current_process->pid);
  removeProcessFromList(current_process);
  free(current_process);

  if (countProcess() == 0)
  {
    TracePrintf(2, "ExitSwitch: no more process, halt the system\n");
    Halt();
  }

  // Exit the pages of the current process
  int i;
  for (i = MEM_INVALID_SIZE; i < VMEM_0_LIMIT; i += PAGESIZE)
    removePageTableEntry(PAGE_TABLE_0_VADDR, i, 1);

  uintptr_t page_table = next_process->page_table;

  WriteRegister(REG_PTR0, (RCS421RegVal)page_table);
  writePageTableEntry(getPageTable1(), (uintptr_t)PAGE_TABLE_0_VADDR, page_table, PROT_READ | PROT_WRITE, PROT_NONE);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  return &next_process->ctx;
}