#ifndef YALNIX_PAGE_H
#define YALNIX_PAGE_H

#include <stdint.h>
// this file manages the page table and the physical memory
// we will always use uintptr_t instead of void*
// because void* is managed by the underlying system
// and could face problem in lower section (due to linux kernel)
// where it will beforce to be NULL

// initialize the page table, because malloc is not available
// we need to pass in a valid address from the caller (kernel)
void initPagePool(unsigned int pmem_size);

// keep track of the free page count
int getPageCount();

// mark this page as used manually
int usePage(uintptr_t addr);

// allocate a page
uintptr_t allocatePage();

// free a page
void freePage(uintptr_t addr);

// allocate multiple pages, and put the addresses in new_pages
// either all pages are allocated, or none of them are allocated
int allocateMultiPage(int page_count, uintptr_t new_pages[page_count]);

// helper to print the page table
void printPagePool();

#endif // YALNIX_PAGE_H