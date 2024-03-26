#ifndef YALNIX_TERMINAL_H
#define YALNIX_TERMINAL_H
#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include "tty_buffer.h"
// this file stores the functions for terminal initialization

#define MAX_BUF_SIZE 1024  // the maximum size of the tty buffers

#define IDLE 0
#define BUSY 1

// initialize the terminal status, and the tty buffers
void initTerminals();

tty_buf **getTtyReceiveBuf();

tty_buf **getTtyTransmitBuf();

int *getTerminalTransmitStatus();

#endif // YALNIX_TERMINAL_H

