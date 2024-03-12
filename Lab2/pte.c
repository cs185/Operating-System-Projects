#include <comp421/hardware.h>
#include <string.h>
#include "page.h"
#include "pte.h"
#include "pcb.h"

static struct pte *page_table_1_vaddr = NULL;

void setPageTable1(struct pte *page_table)
{
  page_table_1_vaddr = page_table;
}

struct pte *getPageTable1()
{
  return page_table_1_vaddr;
}

void printPageTableEntries(struct pte *page_table)
{
  int i;
  for (i = 0; i < PAGE_TABLE_LEN; i++)
  {
    if (page_table[i].valid)
    {
      TracePrintf(0, "page_table[%d (0x%x)]: valid = %d, kprot = %d, uprot = %d, pfn = %d\n", i, i << PAGESHIFT, page_table[i].valid, page_table[i].kprot, page_table[i].uprot, page_table[i].pfn);
    }
  }
}

void writePageTableEntry(struct pte *page_table, uintptr_t virtual_address, uintptr_t physical_address, unsigned int kprot, unsigned int uprot)
{
  // deduce the base address
  int base = virtual_address >= VMEM_1_BASE ? VMEM_1_BASE : VMEM_0_BASE;
  int vpn = (virtual_address - base) >> PAGESHIFT;

  int pfn = -1;
  if (physical_address != (uintptr_t)-1)
  {
    pfn = physical_address >> PAGESHIFT;
    page_table[vpn].pfn = pfn;
  }
  if (kprot != (unsigned int)-1)
  {
    page_table[vpn].kprot = kprot;
  }
  if (uprot != (unsigned int)-1)
  {
    page_table[vpn].uprot = uprot;
  }
  page_table[vpn].valid = 1;

  TracePrintf(4, "writePageTableEntry: virtual address of 0x%x is added to page table at position %d matching physical address of 0x%x with pfn %d\n", virtual_address, vpn, physical_address, pfn);
}

void removePageTableEntry(struct pte *page_table, uintptr_t virtual_address, int free)
{
  // deduce the base address
  int base = virtual_address >= VMEM_1_BASE ? VMEM_1_BASE : VMEM_0_BASE;
  int vpn = (virtual_address - base) >> PAGESHIFT;
  if (page_table[vpn].valid)
  {
    if (free)
      freePage(page_table[vpn].pfn << PAGESHIFT);
    page_table[vpn].valid = 0;
  }
}

int countPageTableEntries()
{
  struct pte *page_table_0_vaddr = PAGE_TABLE_0_VADDR;
  int i;
  int valid_count = 0;
  for (i = 0; i < PAGE_TABLE_LEN; i++)
    if (page_table_0_vaddr[i].valid)
      valid_count++;
  return valid_count;
}

int copyPageTableEntries(uintptr_t dest)
{
  // current process's page table is here
  struct pte *page_table_0_vaddr = PAGE_TABLE_0_VADDR;

  int valid_count = countPageTableEntries();

  // no need to copy if there is no valid entry
  if (valid_count == 0)
    return 0;

  uintptr_t new_pages[valid_count];
  if (allocateMultiPage(valid_count, new_pages) == -1)
  {
    TracePrintf(0, "copyPageTableEntries: out of memory\n");
    return -1;
  }

  // borrow a page from the kernel heap (PAGE_TABLE_HELPER_1_VADDR) to gain access to the new page table
  writePageTableEntry(page_table_1_vaddr, (uintptr_t)PAGE_TABLE_HELPER_1_VADDR, (uintptr_t)dest, PROT_READ | PROT_WRITE, PROT_NONE);
  WriteRegister(REG_TLB_FLUSH, (uintptr_t)PAGE_TABLE_HELPER_1_VADDR);

  int i;
  // use j as pointer to the new_pages
  int j = 0;
  for (i = 0; i < PAGE_TABLE_LEN; i++)
  {
    if (page_table_0_vaddr[i].valid)
    {
      uintptr_t virtual_address = i << PAGESHIFT;
      uintptr_t physical_address = new_pages[j++];
      TracePrintf(4, "virtual_address = 0x%x, physical_address = 0x%x\n", virtual_address, physical_address);
      writePageTableEntry(PAGE_TABLE_HELPER_1_VADDR, virtual_address, physical_address, page_table_0_vaddr[i].kprot, page_table_0_vaddr[i].uprot);
      // borrow another page (PAGE_TABLE_HELPER_2_VADDR) to gain access to the memory to the content of the page
      writePageTableEntry(page_table_1_vaddr, (uintptr_t)PAGE_TABLE_HELPER_2_VADDR, physical_address, PROT_READ | PROT_WRITE, PROT_NONE);
      WriteRegister(REG_TLB_FLUSH, (uintptr_t)PAGE_TABLE_HELPER_2_VADDR);

      // copy the content
      // memcpy((void *)(PAGE_TABLE_HELPER_2_VADDR), (void *)virtual_address, PAGESIZE);
      int k;
      for (k = 0; k < PAGESIZE; k++)
        ((char *)(PAGE_TABLE_HELPER_2_VADDR))[k] = ((char *)(virtual_address))[k];
    }
  }

  // remove the pages from the kernel heap, but leave them in physical memory
  removePageTableEntry(page_table_1_vaddr, (uintptr_t)PAGE_TABLE_HELPER_1_VADDR, 0);
  removePageTableEntry(page_table_1_vaddr, (uintptr_t)PAGE_TABLE_HELPER_2_VADDR, 0);
  WriteRegister(REG_TLB_FLUSH, (uintptr_t)PAGE_TABLE_HELPER_1_VADDR);
  WriteRegister(REG_TLB_FLUSH, (uintptr_t)PAGE_TABLE_HELPER_2_VADDR);

  return 0;
}