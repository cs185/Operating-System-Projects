#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <comp421/hardware.h>
#include <comp421/loadinfo.h>
#include "page.h"
#include "pcb.h"
#include "load.h"
#include "pte.h"

int LoadProgram(char *name, char **args)
{
  int fd;
  int status;
  struct loadinfo li;
  char *cp;
  char *cp2;
  char **cpp;
  char *argbuf;
  int i;
  unsigned long argcount;
  int size;
  int text_npg;
  int data_bss_npg;
  int stack_npg;

  TracePrintf(2, "LoadProgram '%s', args %p\n", name, args);

  if ((fd = open(name, O_RDONLY)) < 0)
  {
    TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
    return (-1);
  }

  status = LoadInfo(fd, &li);
  TracePrintf(3, "LoadProgram: LoadInfo status %d\n", status);
  switch (status)
  {
  case LI_SUCCESS:
    break;
  case LI_FORMAT_ERROR:
    TracePrintf(0,
                "LoadProgram: '%s' not in Yalnix format\n", name);
    close(fd);
    return (-1);
  case LI_OTHER_ERROR:
    TracePrintf(0, "LoadProgram: '%s' other error\n", name);
    close(fd);
    return (-1);
  default:
    TracePrintf(0, "LoadProgram: '%s' unknown error\n", name);
    close(fd);
    return (-1);
  }
  TracePrintf(2, "LoadProgram: text_size 0x%lx, data_size 0x%lx, bss_size 0x%lx, entry 0x%lx\n",
              li.text_size, li.data_size, li.bss_size, li.entry);

  /*
   *  Figure out how many bytes are needed to hold the arguments on
   *  the new stack that we are building.  Also count the number of
   *  arguments, to become the argc that the new "main" gets called with.
   */
  size = 0;
  for (i = 0; args[i] != NULL; i++)
  {
    TracePrintf(0, "LoadProgram: arg '%s'\n", args[i]);
    size += strlen(args[i]) + 1;
  }
  argcount = i;
  TracePrintf(3, "LoadProgram: size %d, argcount %d\n", size, argcount);

  /*
   *  Now save the arguments in a separate buffer in Region 1, since
   *  we are about to delete all of Region 0.
   */
  cp = argbuf = (char *)malloc(size);
  for (i = 0; args[i] != NULL; i++)
  {
    strcpy(cp, args[i]);
    cp += strlen(cp) + 1;
  }

  /*
   *  The arguments will get copied starting at "cp" as set below,
   *  and the argv pointers to the arguments (and the argc value)
   *  will get built starting at "cpp" as set below.  The value for
   *  "cpp" is computed by subtracting off space for the number of
   *  arguments plus 4 (for the argc value, a 0 (AT_NULL) to
   *  terminate the auxiliary vector, a NULL pointer terminating
   *  the argv pointers, and a NULL pointer terminating the envp
   *  pointers) times the size of each (sizeof(void *)).  The
   *  value must also be aligned down to a multiple of 8 boundary.
   */
  cp = ((char *)USER_STACK_LIMIT) - size;
  cpp = (char **)((unsigned long)cp & (-1 << 4)); /* align cpp */
  cpp = (char **)((unsigned long)cpp - ((argcount + 4) * sizeof(void *)));

  text_npg = li.text_size >> PAGESHIFT;
  data_bss_npg = UP_TO_PAGE(li.data_size + li.bss_size) >> PAGESHIFT;
  stack_npg = (USER_STACK_LIMIT - DOWN_TO_PAGE(cpp)) >> PAGESHIFT;

  TracePrintf(3, "LoadProgram: text_npg %d, data_bss_npg %d, stack_npg %d\n",
              text_npg, data_bss_npg, stack_npg);

  /*
   *  Make sure we have enough *virtual* memory to fit everything within
   *  the size of a page table, including leaving at least one page
   *  between the heap and the user stack
   */
  if (MEM_INVALID_PAGES + text_npg + data_bss_npg + 1 + stack_npg +
          1 + KERNEL_STACK_PAGES >
      PAGE_TABLE_LEN)
  {
    TracePrintf(0,
                "LoadProgram: program '%s' size too large for VIRTUAL memory\n",
                name);
    free(argbuf);
    close(fd);
    return (-1);
  }

  /*
   *  And make sure there will be enough *physical* memory to
   *  load the new program.
   */
  // The new program will require text_npg pages of text,
  // data_bss_npg pages of data/bss, and stack_npg pages of
  // stack.  In checking that there is enough free physical
  // memory for this, be sure to allow for the physical memory
  // pages already allocated to this process that will be
  // freed below before we allocate the needed pages for
  // the new program being loaded.

  int page_count = getPageCount();

  struct pte *page_table = PAGE_TABLE_0_VADDR;

  // printPageTableEntries(page_table);
  // TracePrintf(0, "LoadProgram: page_table %x\n", page_table);

  int free_page_count = 0;

  for (i = MEM_INVALID_SIZE; i < USER_STACK_LIMIT; i += PAGESIZE)
  {
    int page_table_index = i >> PAGESHIFT;
    if (page_table[page_table_index].valid)
      free_page_count++;
  }

  TracePrintf(3, "LoadProgram: we can free %d pages\n", free_page_count);

  page_count += free_page_count;

  TracePrintf(3, "LoadProgram: we can use %d pages in total\n", page_count);

  int total_pages = text_npg + data_bss_npg + stack_npg;
  if (total_pages > page_count)
  {
    TracePrintf(0,
                "LoadProgram: program '%s' size too large for PHYSICAL memory\n",
                name);
    free(argbuf);
    close(fd);
    return (-1);
  }

  pcb *current_process = getCurrentProcess();

  //  Initialize sp for the current process to (void *)cpp.
  //  The value of cpp was initialized above.

  current_process->sp = (void *)cpp;
  /*
   *  Free all the old physical memory belonging to this process,
   *  but be sure to leave the kernel stack for this process (which
   *  is also in Region 0) alone.
   */
  // Loop over all PTEs for the current process's Region 0,
  // except for those corresponding to the kernel stack (between
  // address KERNEL_STACK_BASE and KERNEL_STACK_LIMIT).  For
  // any of these PTEs that are valid, free the physical memory
  // memory page indicated by that PTE's pfn field.  Set all
  // of these PTEs to be no longer valid.

  // because we use the page between USER_STACK_LIMIT and KERNEL_STACK_BASE as the page table
  // we cannot free it
  for (i = MEM_INVALID_SIZE; i < USER_STACK_LIMIT; i += PAGESIZE)
    removePageTableEntry(page_table, i, 1);

  /*
   *  Fill in the page table with the right number of text,
   *  data+bss, and stack pages.  We set all the text pages
   *  here to be read/write, just like the data+bss and
   *  stack pages, so that we can read the text into them
   *  from the file.  We then change them read/execute.
   */

  // Leave the first MEM_INVALID_PAGES number of PTEs in the
  // Region 0 page table unused (and thus invalid)

  /* First, the text pages */
  // For the next text_npg number of PTEs in the Region 0
  // page table, initialize each PTE:
  //     valid = 1
  //     kprot = PROT_READ | PROT_WRITE
  //     uprot = PROT_READ | PROT_EXEC
  //     pfn   = a new page of physical memory
  for (i = 0; i < text_npg; i++)
    writePageTableEntry(PAGE_TABLE_0_VADDR, (MEM_INVALID_PAGES + i) << PAGESHIFT, allocatePage(), PROT_READ | PROT_WRITE, PROT_READ | PROT_EXEC);

  /* Then the data and bss pages */
  // For the next data_bss_npg number of PTEs in the Region 0
  // page table, initialize each PTE:
  //     valid = 1
  //     kprot = PROT_READ | PROT_WRITE
  //     uprot = PROT_READ | PROT_WRITE
  //     pfn   = a new page of physical memory
  for (i = 0; i < data_bss_npg; i++)
    writePageTableEntry(PAGE_TABLE_0_VADDR, (MEM_INVALID_PAGES + text_npg + i) << PAGESHIFT, allocatePage(), PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE);

  /* And finally the user stack pages */
  // For stack_npg number of PTEs in the Region 0 page table
  // corresponding to the user stack (the last page of the
  // user stack *ends* at virtual address USER_STACK_LIMIT),
  // initialize each PTE:
  //     valid = 1
  //     kprot = PROT_READ | PROT_WRITE
  //     uprot = PROT_READ | PROT_WRITE
  //     pfn   = a new page of physical memory
  for (i = 0; i < stack_npg; i++)
    writePageTableEntry(PAGE_TABLE_0_VADDR, USER_STACK_LIMIT - ((i + 1) << PAGESHIFT), allocatePage(), PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE);

  /*
   *  All pages for the new address space are now in place.  Flush
   *  the TLB to get rid of all the old PTEs from this process, so
   *  we'll be able to do the read() into the new pages below.
   */
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  // the initial brk for the current process is the end of the data segment
  current_process->brk = (MEM_INVALID_PAGES + text_npg + data_bss_npg) << PAGESHIFT;
  // the initial stack pointer for the current process is the lowest address of the last valid page of the user stack
  current_process->stk = DOWN_TO_PAGE(cpp);
  // current_process->stk = USER_STACK_LIMIT - (stack_npg << PAGESHIFT)

  // there might be something messing up with the page table address, but I don't know what it is
  // TracePrintf(0, "current process page table 0x%x\n", current_process->page_table);

  /*
   *  Read the text and data from the file into memory.
   */
  if (read(fd, (void *)MEM_INVALID_SIZE, li.text_size + li.data_size) != (ssize_t)(li.text_size + li.data_size))
  {
    TracePrintf(0, "LoadProgram: couldn't read for '%s'\n", name);
    free(argbuf);
    close(fd);
    return (-2);
    // Since we are returning - 2 here, this should mean to
    // the rest of the kernel that the current process should
    // be terminated with an exit status of ERROR reported
    // to its parent process.return (-2);
  }

  close(fd); /* we've read it all now */

  /*
   *  Now set the page table entries for the program text to be readable
   *  and executable, but not writable.
   */
  // For text_npg number of PTEs corresponding to the user text
  // pages, set each PTE's kprot to PROT_READ | PROT_EXEC.
  for (i = 0; i < text_npg; i++)
    writePageTableEntry(PAGE_TABLE_0_VADDR, (MEM_INVALID_PAGES + i) << PAGESHIFT, (uintptr_t)-1, PROT_READ | PROT_EXEC, (unsigned int)-1);

  TracePrintf(3, "LoadProgram: set text to read/exec\n");

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  /*
   *  Zero out the bss
   */
  memset((void *)(MEM_INVALID_SIZE + li.text_size + li.data_size),
         '\0', li.bss_size);

  /*
   *  Set the entry point in the ExceptionInfo.
   */
  // Initialize pc for the current process to (void *)li.entry
  current_process->pc = (void *)li.entry;

  TracePrintf(3, "LoadProgram: entry is 0x%lx\n", li.entry);

  /*
   *  Now, finally, build the argument list on the new stack.
   */
  *cpp++ = (char *)argcount; /* the first value at cpp is argc */
  cp2 = argbuf;
  for (i = 0; i < (int)argcount; i++)
  { /* copy each argument and set argv */
    *cpp++ = cp;
    strcpy(cp, cp2);
    cp += strlen(cp) + 1;
    cp2 += strlen(cp2) + 1;
  }
  free(argbuf);
  *cpp++ = NULL; /* the last argv is a NULL pointer */
  *cpp++ = NULL; /* a NULL pointer for an empty envp */
  *cpp++ = 0;    /* and terminate the auxiliary vector */

  /*
   *  Initialize all regs[] registers for the current process to 0,
   *  initialize the PSR for the current process also to 0.  This
   *  value for the PSR will make the process run in user mode,
   *  since this PSR value of 0 does not have the PSR_MODE bit set.
   */
  // Initialize regs[0] through regs[NUM_REGS-1] for the
  // current process to 0.
  // Initialize psr for the current process to 0.

  current_process->psr = 0;
  for (i = 0; i < NUM_REGS; i++)
  {
    current_process->regs[i] = 0;
  }

  TracePrintf(2, "LoadProgram: complete\n");

  return (0);
}
