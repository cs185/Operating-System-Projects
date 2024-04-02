#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "page.h"

// memory size
static unsigned int memory_size;
// we use a bitmap to keep track of free page frames
static int total_pages;
// a char can represent 8 pages
// 1 means free, 0 means used
static unsigned char *free_page_bitmap;
// use for find next free page
// we start with MEM_INVALID_PAGES to avoid problem when virtual memory is not enabled
// it will spin back later, and at that time virtual memory should be enabled
static int page_next = MEM_INVALID_PAGES - 1;
// keep track of the free page count
static int page_count;
// can we use rest half page? if it is -1, we need a new page
// else it shall be the address of the half page
static uintptr_t half_page = (uintptr_t)-1;

// initialize the page table, because malloc is not available
// we need to pass in a valid address from the caller (kernel)
void initPagePool(unsigned int pmem_size)
{
  // initialize the memory size
  memory_size = pmem_size;
  // initialize the total page count
  total_pages = memory_size >> PAGESHIFT;
  int bitmap_size = (total_pages + 7) / 8;
  // initialize the bitmap
  free_page_bitmap = (unsigned char *)malloc(bitmap_size * sizeof(unsigned char));
  page_count = total_pages;
  // initialize the bitmap
  // fill all bit as 1
  int i;

  for (i = 0; i < bitmap_size - 1; i++)
    free_page_bitmap[i] = 0xFF;

  // for the last byte, we need to set the bits to 1 based on the total_pages
  int last_byte = total_pages % 8;
  unsigned char mask = 0xFF;
  if (last_byte != 0)
    for (i = 0; i < 8 - last_byte; i++)
      mask = mask << 1;
  free_page_bitmap[bitmap_size - 1] = mask;
}

int getPageCount()
{
  return page_count;
}

// utility function to check if a page is free
static int isPageFree(int index)
{
  int bit_index = index / 8;
  unsigned char mask = 1 << (index % 8);
  return free_page_bitmap[bit_index] & mask;
}

// utility function to mark a page as free
static void markPageFree(int index)
{
  int bit_index = index / 8;
  // set the bit to mark as free
  unsigned char mask = 1 << (index % 8);
  free_page_bitmap[bit_index] |= mask;
  page_count++;
}

// utility function to mark a page as used
static void markPageUsed(int index)
{
  int byteIndex = index / 8;
  // set the bit to mark as used
  unsigned char bitMask = 1 << (index % 8);
  free_page_bitmap[byteIndex] &= ~bitMask;
  page_count--;
}

// utility to find the next free page, starting from page_nextIndex
static int findFreePage()
{
  // check before hand
  if (page_count < 1)
    return -1;
  int i;
  for (i = 0; i < total_pages; i++)
  {
    page_next = (page_next + 1) % total_pages;
    if (isPageFree(page_next))
    {
      return page_next;
    }
  }

  // just in case
  return -1;
}

// use the page at the given address
int usePage(uintptr_t addr)
{
  int index = addr >> PAGESHIFT;
  if (index < 0 || index >= total_pages)
  {
    TracePrintf(0, "usePage: invalid address 0x%x with page index %d\n", addr, index);
    return -1;
  }
  // if the page is already in use, do nothing
  if (!isPageFree(index))
  {
    TracePrintf(0, "usePage: address 0x%x with page index %d is already in use\n", addr, index);
    return -1;
  }
  markPageUsed(index);
  // TracePrintf(3, "usePage: address 0x%x with page index %d is used\n", addr, index);
  return 0;
}

// allocate a page
uintptr_t allocatePage()
{
  int index = findFreePage();
  if (index == -1)
  {
    TracePrintf(0, "allocatePage: out of memory\n");
    return -1;
  }

  uintptr_t page = (uintptr_t)(index << PAGESHIFT);
  markPageUsed(index);
  // TracePrintf(3, "allocatePage: allocated page %d, with address 0x%x\n", index, page);
  return page;
}

// free a page
void freePage(uintptr_t addr)
{
  int index = addr >> PAGESHIFT;
  // this should not happen, but just in case
  if (index < 0 || index >= total_pages)
  {
    TracePrintf(0, "freePage: invalid page index %d\n", index);
    return;
  }

  markPageFree(index);
  // TracePrintf(3, "freePage: page 0x%x with index %d is freed\n", addr, index);
}

// allocate multiple pages, and put the addresses in new_pages
// either all pages are allocated, or none of them are allocated
int allocateMultiPage(int new_page_count, uintptr_t new_pages[new_page_count])
{
  // we can do the check before hand
  if (page_count < new_page_count)
  {
    TracePrintf(0, "allocateMultiPage: out of memory\n");
    return -1;
  }
  int i;
  for (i = 0; i < new_page_count; i++)
  {
    new_pages[i] = allocatePage();
    // this shall not happen, but just in case
    if (new_pages[i] == (uintptr_t)-1)
    {
      // we need to free all the pages we have added
      int j;
      for (j = 0; j < i; j++)
      {
        freePage(new_pages[j]);
      }
      TracePrintf(0, "allocateMultiPage: out of memory\n");
      return -1;
    }
  }
  TracePrintf(3, "allocateMultiPage: allocated %d pages\n", new_page_count);
  return 0;
}

// helper to print the page table
void printPagePool()
{
  int i;
  for (i = 0; i < total_pages; i += 8)
  {
    char str[] = "        ";
    int j;
    for (j = 0; j < 8; j++)
    {
      str[j] = isPageFree(i + j) ? '1' : '0';
    }
    TracePrintf(0, "%d-%d: %s\n", i, i + 7, str);
  }
}

uintptr_t allocateHalfPage()
{
  uintptr_t page = half_page;
  if (half_page == (uintptr_t)-1)
  // if there is no half page available, we need to allocate a new page
  {
    page = allocatePage();
    if (page == (uintptr_t)-1)
      // if we fail, we simply do nothing
      return (uintptr_t)-1;

    half_page = page + PAGESIZE / 2;
  }
  else
  // else we will use the half page
  {
    page = half_page;
    half_page = (uintptr_t)-1;
  }
  return page;
}