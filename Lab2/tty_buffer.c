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
#include "tty_buffer.h"

tty_buf *createBuffer() 
{
    tty_buf *buf = (tty_buf *)malloc(sizeof(tty_buf));

    if (buf == NULL) 
    {
        TracePrintf(1, "memory allocation failed\n");
        return NULL;
    }

    // allocate memory from region 1 to create the array for the chars
    buf->items = (char *)malloc(INITIAL_CAPACITY * sizeof(char));
    if (buf->items == NULL) 
    {
        free(buf);
        return NULL;
    }

    // initialize the variables
    buf->front = 0;
    buf->rear = 0;
    buf->size = 0;
    buf->capacity = INITIAL_CAPACITY;

    return buf;
}

int isFull(tty_buf *buf) 
{
    return buf->size == buf->capacity;
}

int isEmpty(tty_buf *buf) 
{
    return buf->size == 0;
}

void insert(tty_buf *buf, char item) 
{
    // ensure the buf is not full, if it is, grow it
    if (isFull(buf))
        growBuffer(buf);

    buf->items[buf->rear] = item;
    buf->rear = (buf->rear + 1) % buf->capacity;
    buf->size++;
}

char delete(tty_buf *buf) 
{
    char item = '\0';
    if (!isEmpty(buf)) 
    {
        item = buf->items[buf->front];
        buf->front = (buf->front + 1) % buf->capacity;
        buf->size--;
    }
    return item;
}

int addBuf(tty_buf *target_buf, void *buf, int len) 
{
    char *charBuf = (char *)buf;
    int count = 0;
    // keep calling insert function to add the item from buf one by one
    while (!isFull(target_buf) && count < len) 
    {
        insert(target_buf, charBuf[count++]);
    }
    return count;
}

int getBuf(tty_buf *target_buf, void *buf, int len, int get_line) 
{
    char *charBuf = (char *)buf;
    int count = 0;
    // keep calling delete function to pop the item from the target tty buf one by one
    while (!isEmpty(target_buf) && count < len) 
    {
        char item = delete(target_buf);
        charBuf[count++] = item;

        // if meeting the nextline and the get_line is set, finish reading
        if (get_line && item == '\n') 
        {
            break;
        }
    }
    return count;
}

void freeBuffer(tty_buf *buf) 
{
    if (buf) 
    {
        free(buf->items);
        free(buf);
    }
}

void growBuffer(tty_buf *buf) 
{
    int new_capacity = buf->capacity * GROWTH_FACTOR;
    char *new_items = realloc(buf->items, new_capacity * sizeof(char));
    if (new_items == NULL) 
    {
        TracePrintf(0, "grow buffer failed\n");
        return;
    }

    if (buf->front > buf->rear) 
    {
        // if the data in tty buffer is segmented, rearrange it to make it consistence with the logic
        // that is to put the second segmentation (0, rear) at the end of the first one (front, capacity)
        memcpy(new_items + buf->capacity, new_items, buf->rear * sizeof(char));
        buf->rear += buf->capacity;
    }

    buf->items = new_items;
    buf->capacity = new_capacity;
}


