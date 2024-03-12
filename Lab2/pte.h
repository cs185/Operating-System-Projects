#ifndef YALNIX_PTE_H
#define YALNIX_PTE_H

#include <stdint.h>
#include <comp421/hardware.h>
// this file stores the utils corresponding to the page table entry

// the page table itself of region 0 is at the end of the kernel's virtual memory
#define PAGE_TABLE_0_VADDR (struct pte *)(VMEM_1_LIMIT - PAGESIZE)
// a temporary page to help with the page table creation
#define PAGE_TABLE_HELPER_1_VADDR (struct pte *)(VMEM_1_LIMIT - 2 * PAGESIZE)
// another temporary page to help with the page table creation
#define PAGE_TABLE_HELPER_2_VADDR (struct pte *)(VMEM_1_LIMIT - 3 * PAGESIZE)

// a utility function to print the page table entries
void printPageTableEntries(struct pte *page_table);

// a utility function to help writing to each invidual page table entry
// if any number passed in is -1, we will not write to that field (virtual_address is a must)
void writePageTableEntry(struct pte *page_table, uintptr_t virtual_address, uintptr_t physical_address, unsigned int kprot, unsigned int uprot);

// a utility function to remove a page table entry
// and free the physical memory if needed
void removePageTableEntry(struct pte *page_table, uintptr_t virtual_address, int free);

// copy the page table entries from current process's page table to dest page table
// return -1 if failed, 0 if success
int copyPageTableEntries(uintptr_t dest);

// count how many page is used for region 0
int countPageTableEntries();

// set the region 1 page table address (virtual address) for later use
void setPageTable1(struct pte *page_table);

// get the region 1 page table address (virtual address)
struct pte *getPageTable1();

#endif // YALNIX_PTE_H
