#ifndef YALNIX_TTY_BUFFER_H
#define YALNIX_TTY_BUFFER_H
#include <comp421/hardware.h>
#include <comp421/yalnix.h>
// this file stores the tty buffer functions

#define INITIAL_CAPACITY 1024
#define GROWTH_FACTOR 2

typedef struct {
    char *items;
    int front;
    int rear;
    int size;
    int capacity;
} tty_buf;

tty_buf* createBuffer();

int isFull(tty_buf *buf);

int isEmpty(tty_buf *buf);

// get a set of chars from the target tty buf and store them in buf and delete these chars from the buff
// if get_line is set, this function will read the chars until the next '\n'
int getBuf(tty_buf *target_buf, void *buf, int len, int get_line);

// add a buf of length len to the target tty buf
int addBuf(tty_buf *target_buf, void *buf, int len);

// insert a char to the tty_buf
void insert(tty_buf *buf, char item);

// pop out the item pointed by front from the tty_buf
char delete(tty_buf *buf);

// free the tty buffer
void freeBuffer(tty_buf *buf);

// dynamically grow the size of the buffer
void growBuffer(tty_buf *buf);

#endif // YALNIX_TTY_BUFFER_H
