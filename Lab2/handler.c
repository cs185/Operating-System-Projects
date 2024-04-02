#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "handler.h"
#include "pcb.h"
#include "switch.h"
#include "page.h"
#include "pte.h"
#include "load.h"
#include "exit_status.h"
#include "terminal.h"
#include "tty_buffer.h"

static int clock_ticks = 0;

// every process has 2 clock ticks
#define CLOCK_INTERVAL 2

// check if the address is valid for the user for the specific protection
// return 1 is valid, 0 is invalid
static int validateAddr(uintptr_t addr, int prot)
{
  if (addr >= USER_STACK_LIMIT)
  {
    // this is a immediate failure
    TracePrintf(5, "validateAddr: address is outside of user space\n");
    return 0;
  }
  // go to the page table to check if the address is valid for read and write by the user
  int page = addr >> PAGESHIFT;

  struct pte *page_table = (struct pte *)PAGE_TABLE_0_VADDR;

  if ((page_table[page].valid == 0) || ((page_table[page].uprot & prot) != prot))
  {
    TracePrintf(5, "validateAddr: address 0x%x is not valid for protection\n", addr);
    return 0;
  }

  return 1;
}

// we need to becareful about any kinds of pointers
// that user passed in, we need to validate them
// before we use them, else user can easily crash the kernel
// by passing in address of kernel space
// return 1 is valid, 0 is invalid
static int validatePointer(uintptr_t addr, int len, int prot)
{
  if (len < 0)
  {
    TracePrintf(5, "validatePointer: buffer length is negative\n");
    return 0;
  }

  if (len == 0)
  {
    // length is 0, we will not check the buffer
    // auto pass
    return 1;
  }

  // we will do the check page by page, to speed up

  // we need to check if the buffer is in the user space
  uintptr_t current_addr = addr;

  uintptr_t end = addr + len;

  while (current_addr < end)
  {
    if (validateAddr(current_addr, prot) == 0)
      return 0;
    // we are safe to read the buffer for this page
    // so we can move to the next page
    current_addr += PAGESIZE;
    current_addr = DOWN_TO_PAGE(current_addr);
  }

  return 1;
}

static int validateString(uintptr_t addr, int prot)
{
  uintptr_t current_addr = DOWN_TO_PAGE(addr);

  // we will check for the validity of the page first
  // then after the check we read through the page to find out the end of the string
  while (1)
  {
    if (validateAddr(current_addr, prot) == 0)
      return 0;

    uintptr_t end = UP_TO_PAGE(addr);

    // we are safe to read the buffer for this page
    // so we can move to the next page
    while (current_addr < end)
    {
      char *c = (char *)current_addr;
      if (*c == '\0')
        return 1;
      current_addr++;
    }
    // now we are at the start of the next page
  }

  return 1;
}

static void writeToTerminal(int tty_id, void *buf, int len)
{
  struct pcb *current_process = getCurrentProcess();
  current_process->tty_write_id = tty_id;
  int trans_index = 0;
  while (trans_index < len)
  {
    if (getTerminalTransmitStatus(tty_id) == BUSY)
    {
      struct pcb *next_process = getNextProcess(0);
      TracePrintf(3, "blocked the writing process with pid=%d, switching to next process with pid=%d\n", current_process->pid, next_process->pid);
      // remove it from the execution list
      removeProcessFromList(current_process);
      addProcessToList(current_process, TTY_WRITE_LIST);

      ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
      TracePrintf(3, "switched back in TtyWrite, now process with pid=%d able to write to terminal %d\n", current_process->pid, tty_id);
    }

    int trans_index_next = trans_index + TERMINAL_MAX_LINE;
    if (trans_index_next > len)
      trans_index_next = len;

    int trans_len = trans_index_next - trans_index;

    TracePrintf(3, "string too long, transmiting the %d chars first\n", trans_len);
    // int transmit_len = getBuf(tty_transmit_buf, buf, TERMINAL_MAX_LINE, 0);
    // TracePrintf(3, "get %d chars from tty_transmit_buf, the size of tty_transmit_buf now: %d\n", transmit_len, tty_transmit_buf->size);
    TtyTransmit(tty_id, buf + trans_index, trans_len);
    setTerminalTransmitStatus(tty_id, BUSY);

    trans_index = trans_index_next;
  }

  if (getTerminalTransmitStatus(tty_id) == BUSY)
  {
    struct pcb *next_process = getNextProcess(0);
    TracePrintf(3, "blocked the writing process with pid=%d, switching to next process with pid=%d\n", current_process->pid, next_process->pid);
    // remove it from the execution list
    removeProcessFromList(current_process);
    addProcessToList(current_process, TTY_WRITE_LIST);

    ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
    TracePrintf(3, "switched back in TtyWrite, now process with pid=%d able to write to terminal %d\n", current_process->pid, tty_id);
  }

  TracePrintf(3, "TtyWrite for process with pid=%d done\n", current_process->pid);
  current_process->tty_write_id = -1;
}

static void writeStrToTerminal(int tty_id, char *str)
{
  int len = strlen(str);
  writeToTerminal(tty_id, str, len);
}

// utility function to exit the current process and get the next one running
static void exitProcess(int status)
{
  TracePrintf(2, "exitProcess: exit process is called\n");
  struct pcb *current_process = getCurrentProcess();
  struct pcb *next_process = getNextProcess(0);
  struct pcb *wait_process = getList(WAIT_LIST);
  wait_process = wait_process->next;
  while (wait_process->pid >= 0)
  {
    // if the parent process is in the wait list
    if (wait_process->pid == current_process->ppid)
    {
      // we will wake up the parent process
      removeProcessFromList(wait_process);
      addProcessToList(wait_process, EXECUTION_LIST);
      // there shall only be one parent process
      break;
    }
    wait_process = wait_process->next;
  }

  // though this kind of duplicated with the above code, we will still do it this way
  struct pcb *parent_process = getProcessByPid(current_process->ppid);
  if (parent_process != NULL)
  {
    parent_process->child_count--;

    // only when the parent still exist, we will add the exit status (to avoid useless exit status)
    addExitStatus(current_process->ppid, current_process->ppid, status);
  }

  int dummy_status;
  // remove the exit status of the current process's children
  removeExitStatus(current_process->pid, &dummy_status);

  ContextSwitch(ExitSwitch, &current_process->ctx, current_process, next_process);
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
    int page_count = countPageTableEntries();
    if (getPageCount() < page_count)
    {
      TracePrintf(0, "onTrapKernel: not enough pages to allocate for fork\n");
      info->regs[0] = -1;
      break;
    }

    // we will create a new process
    // and also make the new page table ready
    struct pcb *new_process = createProcess();
    // uintptr_t page_table = allocateHalfPage();
    uintptr_t page_table = allocatePage();
    if (page_table == (uintptr_t)-1)
    {
      TracePrintf(0, "onTrapKernel: failed to allocate page table for fork\n");
      info->regs[0] = -1;
      break;
    }
    new_process->page_table = page_table;

    // we need to copy the usage information of the page table
    new_process->brk = current_process->brk;
    new_process->stk = current_process->stk;
    addProcessToList(new_process, EXECUTION_LIST);
    new_process->ppid = current_process->pid;
    current_process->child_count++;

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

    if (!validateString((uintptr_t)filename, PROT_READ))
    {
      TracePrintf(0, "onTrapKernel: filename is invalid\n");
      writeStrToTerminal(TTY_CONSOLE, "Invalid file name\n");
      info->regs[0] = ERROR;
      break;
    }

    int i;
    for (i = 0; args[i] != NULL; i++)
    {
      if (!validateString((uintptr_t)args[i], PROT_READ))
      {
        TracePrintf(0, "onTrapKernel: arg %d is invalid\n", i);
        writeStrToTerminal(TTY_CONSOLE, "Invalid argument\n");
        info->regs[0] = ERROR;
        break;
      }
    }

    LoadProgram(filename, args);
    struct pcb *current_process = getCurrentProcess();
    info->pc = current_process->pc;
    info->sp = current_process->sp;

    // dont forget to copy (clean up) the registers
    for (i = 0; i < NUM_REGS; i++)
      info->regs[i] = current_process->regs[i];

    // printPageTableEntries(PAGE_TABLE_0_VADDR);
    // printPagePool();

    TracePrintf(2, "current pc is 0x%x, current sp is 0x%x\n", info->pc, info->sp);
    break;
  }
  case YALNIX_EXIT:
  {
    struct pcb *current_process = getCurrentProcess();
    struct pcb *next_process = getNextProcess(0);

    TracePrintf(2, "onTrapKernel: exit is called, current process is %d, next process is %d\n", current_process->pid, next_process->pid);
    int status = (int)info->regs[1];
    exitProcess(status);
    break;
  }
  case YALNIX_WAIT:
  {
    TracePrintf(2, "onTrapKernel: wait is called\n");
    // int *status = (int *)info->regs[1];
    // if there is already a child process exited, we will return the status
    // else we will add the current process to the wait list
    // int *status = (int *)info->regs[1];
    // if there is already a child process exited, we will return the status
    // else we will add the current process to the wait list

    struct pcb *current_process = getCurrentProcess();
    int *status = (int *)info->regs[1];

    if (!validatePointer((uintptr_t)status, sizeof(int), PROT_READ | PROT_WRITE))
    {
      TracePrintf(0, "onTrapKernel: wait status buffer is invalid\n");
      writeStrToTerminal(TTY_CONSOLE, "Invalid address\n");
      info->regs[0] = ERROR;
      break;
    }
    int pid = -1;

    do
    {
      pid = removeExitStatus(current_process->pid, status);
      if (pid == -1)
      {
        // if there is no child process exited
        // we will need to verify this process has children
        if (current_process->child_count > 0)
        {
          struct pcb *next_process = getNextProcess(0);
          // if there are children, we will add the current process to the wait list
          removeProcessFromList(current_process);
          addProcessToList(current_process, WAIT_LIST);
          printList(WAIT_LIST);
          printList(EXECUTION_LIST);
          // we will switch to the next process
          ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
          // after switching back, we will reenter the loop
        }
        else
        {
          // if there is no child process, we will return -1
          writeStrToTerminal(TTY_CONSOLE, "No children left\n");
          info->regs[0] = ERROR;
          break;
        }
      }
      else
      {
        info->regs[0] = pid;
        break;
      }
    } while (pid == -1);

    break;
  }
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
    // we will leave a page between the brk and the stack
    // in this case we will not allocate the page
    if (next_brk + PAGESIZE > current_process->stk)
    {
      TracePrintf(0, "onTrapKernel: out of virtual memory\n");
      info->regs[0] = -1;
      break;
    }
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
    int delay = (int)info->regs[1];
    if (delay <= 0)
    {
      TracePrintf(0, "onTrapKernel: delay time is invalid\n");
      break;
    }
    // we will remove the current process from the execution list
    // and add it to the delay list, then execute the next process
    struct pcb *current_process = getCurrentProcess();
    struct pcb *next_process = getNextProcess(0);
    current_process->delay = delay;

    TracePrintf(2, "onTrapKernel: delay is called, current process is %d, next process is %d\n", current_process->pid, next_process->pid);
    removeProcessFromList(current_process);
    addProcessToList(current_process, DELAY_LIST);
    printList(DELAY_LIST);
    printList(EXECUTION_LIST);
    ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
    break;
  case YALNIX_TTY_READ:
  {
    int tty_id = (int)info->regs[1];
    void *buf = (void *)info->regs[2];
    int len = (int)info->regs[3];

    if (!validatePointer((uintptr_t)buf, len * sizeof(char), PROT_READ | PROT_WRITE))
    {
      TracePrintf(0, "onTrapKernel: tty read buffer is invalid\n");
      writeStrToTerminal(TTY_CONSOLE, "Invalid read buffer\n");
      info->regs[0] = ERROR;
      break;
    }

    TracePrintf(2, "onTrapKernel: tty read is called for terminal %d with target buffer at %x with size=%d\n", tty_id, (uintptr_t)buf, len);

    if (!len)
    {
      info->regs[0] = 0;
      break;
    }

    struct pcb *current_process = getCurrentProcess();
    struct pcb *next_process = getNextProcess(0);

    tty_buf *tty_receive_buf = getTtyReceiveBuf(tty_id);

    current_process->tty_read_id = tty_id;
    // block the current reading process
    if (isEmpty(tty_receive_buf))
    {
      TracePrintf(3, "terminal %d has nothing to read from yet, blocking the reading process with pid=%d\n", tty_id, current_process->pid);
      // remove it from the execution list
      removeProcessFromList(current_process);
      addProcessToList(current_process, TTY_READ_LIST);

      TracePrintf(3, "blocked the reading process with pid=%d, switching to next process with pid=%d\n", current_process->pid, next_process->pid);
      ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
      TracePrintf(3, "switched back in TtyRead, now process with pid=%d able to read terminal %d\n", current_process->pid, tty_id);
    }

    // when context switch back or even not go to the above if
    // there must be something to read
    len = getBuf(tty_receive_buf, buf, len, 1);
    // return error if the char fails to write into the tty buffer
    TracePrintf(3, "read %d chars from tty_receive_buf, the size of tty_receive_buf now: %d\n", len, tty_receive_buf->size);
    current_process->tty_read_id = -1;

    // return error if the char fails to write into the user buffer
    info->regs[0] = len ? len : ERROR;
    break;
  }
  case YALNIX_TTY_WRITE:
  {
    int tty_id = (int)info->regs[1];
    void *buf = (void *)info->regs[2];
    int len = (int)info->regs[3];

    if (!validatePointer((uintptr_t)buf, len * sizeof(char), PROT_READ | PROT_WRITE))
    {
      TracePrintf(0, "onTrapKernel: tty write buffer is invalid\n");
      writeStrToTerminal(TTY_CONSOLE, "Invalid write buffer\n");
      info->regs[0] = ERROR;
      break;
    }

    TracePrintf(2, "onTrapKernel: tty write is called on terminal %d, with %d chars at address %x\n", tty_id, len, (uintptr_t)buf);

    if (!len)
    {
      info->regs[0] = 0;
      break;
    }

    // block the current writing process

    // put it into a kernel buffer first
    void *kernel_buf = malloc(len);
    memcpy(kernel_buf, buf, len);

    writeToTerminal(tty_id, kernel_buf, len);
    info->regs[0] = len;
    break;
  }

  default:
    TracePrintf(0, "onTrapKernel: unknown system call is called\n");
    break;
  }
}

void onTrapClock(ExceptionInfo *info)
{
  AVOID_UNUSED_WARNING(info);
  TracePrintf(2, "onTrapClock: clock interrupt is called\n");
  // decrement the delay time of the process in the delay list
  struct pcb *current_delay_process = getList(DELAY_LIST);

  // ignore the head
  current_delay_process = current_delay_process->next;

  while (current_delay_process->pid >= 0)
  {
    current_delay_process->delay--;
    if (current_delay_process->delay <= 0)
    {
      // reset it just in case
      current_delay_process->delay = 0;
      removeProcessFromList(current_delay_process);
      addProcessToList(current_delay_process, EXECUTION_LIST);

      printList(DELAY_LIST);
      printList(EXECUTION_LIST);
    }
    current_delay_process = current_delay_process->next;
  }

  clock_ticks++;
  if (clock_ticks == CLOCK_INTERVAL)
  {
    clock_ticks = 0;
    struct pcb *current_process = getCurrentProcess();
    struct pcb *next_process = getNextProcess(1);

    TracePrintf(2, "onTrapClock: clock interrupt is called, current process is %d, next process is %d\n", current_process->pid, next_process->pid);

    // if the next process is not the current process, we will do the context switch
    if (next_process != current_process)
      ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_process);
  }
}

void onTrapIllegal(ExceptionInfo *info)
{
  AVOID_UNUSED_WARNING(info);
  TracePrintf(2, "onTrapIllegal: illegal instruction is called\n");

  writeStrToTerminal(TTY_CONSOLE, "Illegal instruction\n");
  exitProcess(ERROR);
}

void onTrapMemory(ExceptionInfo *info)
{
  TracePrintf(2, "onTrapMemory: memory exception is called\n");
  struct pcb *current_process = getCurrentProcess();

  uintptr_t valid_stack_pointer = current_process->stk;

  TracePrintf(2, "accessing memory at 0x%x, while current stack is ar 0x%x\n", info->addr, valid_stack_pointer);

  uintptr_t addr = (uintptr_t)info->addr;

  // if the address is beyond the user stack, terminate the process. Unlikely to happen
  if (addr >= USER_STACK_LIMIT)
  {
    TracePrintf(0, "Fail to allocate new page: illegal memory\n");
    writeStrToTerminal(TTY_CONSOLE, "Segmentation fault\n");
    exitProcess(ERROR);
    return;
  }

  // if the address is within allocated address of the user stack, terminate the process. Unlikely to happen
  if (addr >= valid_stack_pointer)
  {
    TracePrintf(0, "Fail to allocate new page: memory is already available\n");
    writeStrToTerminal(TTY_CONSOLE, "Segmentation fault\n");
    exitProcess(ERROR);
    return;
  }

  uintptr_t next_stk = (uintptr_t)DOWN_TO_PAGE(addr);

  // if the actual being allocated address is in red zone, terminate the process
  if (next_stk < getCurrentProcess()->brk + PAGESIZE)
  {
    TracePrintf(0, "Fail to allocate new page: not enough virtual memory\n");
    writeStrToTerminal(TTY_CONSOLE, "Segmentation fault\n");
    exitProcess(ERROR);
    return;
  }

  // compute the number of pages needed
  // which decides using allocatePage or allocateMultiPage
  int page_count = (int)((valid_stack_pointer - next_stk) >> PAGESHIFT);

  uintptr_t new_pages[page_count];
  if (allocateMultiPage(page_count, new_pages) == -1)
  {
    TracePrintf(0, "Fail to allocate new page: not enough physical memory\n");
    writeStrToTerminal(TTY_CONSOLE, "Segmentation fault\n");
    exitProcess(ERROR);
    return;
  }

  TracePrintf(2, "allocated %d pages to the user stack\n", page_count);
  // insert the new allocated pages into page table
  int i;
  for (i = 0; i < page_count; i++)
  {
    uintptr_t virtual_addr = next_stk + (i << PAGESHIFT);
    writePageTableEntry(PAGE_TABLE_0_VADDR, virtual_addr, new_pages[i], PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE);
  }

  // current_process->sp = (void *)addr;
  // update the sp and stk pointer
  current_process->stk = next_stk;
  // TracePrintf(0, "update process sp at 0x%x\n", addr);
  TracePrintf(0, "update process stk at 0x%x\n", next_stk);
}

void onTrapMath(ExceptionInfo *info)
{
  TracePrintf(2, "onTrapMath: math exception is called with problem of %d\n", info->code);
  // it's mostly same as calling exit
  writeStrToTerminal(TTY_CONSOLE, "Math exception\n");
  exitProcess(ERROR);
}

void onTrapTTYReceive(ExceptionInfo *info)
{
  TracePrintf(2, "onTrapTTYReceive: tty receive is called by terminal %d\n", info->code);

  int tty_id = info->code;

  tty_buf *tty_receive_buf = getTtyReceiveBuf(tty_id);

  void *buf = malloc(TERMINAL_MAX_LINE);
  int str_len = TtyReceive(tty_id, buf, TERMINAL_MAX_LINE);

  int len = addBuf(tty_receive_buf, buf, str_len);
  TracePrintf(3, "added %d out of %d chars to tty_receive_buf, tty_receive_buf size: %d\n", len, str_len, tty_receive_buf->size);
  free(buf);

  struct pcb *tty_read_process = getList(TTY_READ_LIST);
  struct pcb *next_read_process = NULL;
  tty_read_process = tty_read_process->next;
  while (tty_read_process->pid != -1)
  {
    if (tty_read_process->tty_read_id == tty_id)
    {
      next_read_process = tty_read_process;
      break;
    }
    tty_read_process = tty_read_process->next;
  }

  // struct pcb *next_read_process = getList(TTY_READ_LIST);
  // next_read_process = next_read_process->next;
  // while (next_read_process->tty_read_id != tty_id)
  // {
  //   next_read_process = next_read_process->next;
  // }

  if (next_read_process == NULL)
    return;

  TracePrintf(3, "next pending reading process with pid=%d\n", next_read_process->pid);
  // unblock the next process that wants to read, but not neccessarily at once
  removeProcessFromList(next_read_process);

  addProcessToList(next_read_process, EXECUTION_LIST);
  TracePrintf(3, "added the pending reading process to the execution list\n");

  // // if the current process is idle process, switch to the reading process at once
  // struct pcb *current_process = getCurrentProcess();
  // if (current_process->pid == 0)
  // {
  //   TracePrintf(3, "idle process detacted, switch now\n");
  //   ContextSwitch(NormalSwitch, &current_process->ctx, current_process, next_read_process);
  // }
  // if there is no reading process pending, do nothing as we already save the line
}

void onTrapTTYTransmit(ExceptionInfo *info)
{
  TracePrintf(2, "onTrapTTYTransmit: transmission finished by terminal %d\n", info->code);

  int tty_id = info->code;

  setTerminalTransmitStatus(tty_id, IDLE);

  struct pcb *tty_write_process = getList(TTY_WRITE_LIST);
  struct pcb *next_write_process = NULL;
  printList(TTY_WRITE_LIST);
  tty_write_process = tty_write_process->next;
  while (tty_write_process->pid != -1)
  {
    if (tty_write_process->tty_write_id == tty_id)
    {
      next_write_process = tty_write_process;
      break;
    }
    tty_write_process = tty_write_process->next;
  }

  // we have no candidate, we just exit
  if (next_write_process == NULL)
    return;

  TracePrintf(3, "next pending reading process with pid=%d\n", next_write_process->pid);
  // unblock the next process that wants to read, but not neccessarily at once
  removeProcessFromList(next_write_process);

  addProcessToList(next_write_process, EXECUTION_LIST);
  TracePrintf(3, "added the pending reading process to the execution list\n");
  // tty_buf *tty_transmit_buf = getTtyTransmitBuf(tty_id);

  // // always trying to read the maximum of chars
  // void *buf = malloc(TERMINAL_MAX_LINE);
  // // the actual length of the string get from the tty_transmit_buf, len <= TERMINAL_MAX_LINE
  // int len = getBuf(tty_transmit_buf, buf, TERMINAL_MAX_LINE, 0);
  // TracePrintf(3, "get %d chars from tty_transmit_buf, the size of tty_transmit_buf now: %d\n", len, tty_transmit_buf->size);

  // if (len)
  // {
  //   TracePrintf(3, "continue transmiting %d chars from tty_transmit_buf\n", len);
  //   TtyTransmit(tty_id, buf, len);
  //   setTerminalTransmitStatus(tty_id, BUSY);
  // }
  // free(buf);
}

// {
//   tty_buf *tty_transmit_buf = getTtyTransmitBuf(tty_id);

//   if (getTerminalTransmitStatus(tty_id) == BUSY)
//   {
//     TracePrintf(3, "terminal %d is busy transmitting\n", tty_id);
//     len = addBuf(tty_transmit_buf, buf, len);
//     TracePrintf(3, "added %d chars to tty_transmit_buf, tty_transmit_buf size: %d\n", len, tty_transmit_buf->size);
//   }
//   else if (len > TERMINAL_MAX_LINE)
//   {
//     TracePrintf(3, "string too long, transmiting the first part of %d\n", TERMINAL_MAX_LINE);
//     len = addBuf(tty_transmit_buf, buf, len - TERMINAL_MAX_LINE);
//     TracePrintf(3, "added %d chars to tty_transmit_buf, tty_transmit_buf size: %d\n", len, tty_transmit_buf->size);
//     TtyTransmit(tty_id, buf, TERMINAL_MAX_LINE);
//     setTerminalTransmitStatus(tty_id, BUSY);
//   }
//   else
//   {
//     int buf_len = strlen(buf);
//     len = len <= buf_len ? len : buf_len;
//     TracePrintf(3, "transmiting %d chars\n", len);
//     TtyTransmit(tty_id, buf, len);
//     setTerminalTransmitStatus(tty_id, BUSY);
//   }

//   return len;
// }
