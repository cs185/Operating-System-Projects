#ifndef YALNIX_TERMINAL_H
#define YALNIX_TERMINAL_H
#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include "tty_buffer.h"
// this file stores the functions for terminal initialization

enum TTY_STATUS
{
  IDLE,
  BUSY,
};

// initialize the terminal status, and the tty buffers
void initTerminals();

// todo: get by id
tty_buf *getTtyReceiveBuf(int tty_id);

tty_buf *getTtyTransmitBuf(int tty_id);

int getTerminalTransmitStatus(int tty_id);

void setTerminalTransmitStatus(int tty_id, enum TTY_STATUS status);

#endif // YALNIX_TERMINAL_H

