#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <stdio.h>
#include <stdlib.h>
#include "terminal.h"
#include "tty_buffer.h"

// keep track of the status for tranmitting each terminal
int *terminal_transmit_status = NULL;

// the buffers for tty receive and transmit
tty_buf **tty_receive_buf = NULL;
tty_buf **tty_transmit_buf = NULL;

void initTerminals()
{
    terminal_transmit_status = malloc(NUM_TERMINALS * sizeof(int));

    // assign a buf inside region 1 of TERMINAL_MAX_LINE bytes to store the chars received by each terminal
    tty_receive_buf = (tty_buf **)malloc(NUM_TERMINALS * sizeof(tty_buf *));
    tty_transmit_buf = (tty_buf **)malloc(NUM_TERMINALS * sizeof(tty_buf *));

    int i = 0;
    for (i = 0; i < NUM_TERMINALS; i++)
    {
        terminal_transmit_status[i] = IDLE;
        tty_receive_buf[i] = createBuffer(MAX_BUF_SIZE);
        tty_transmit_buf[i] = createBuffer(MAX_BUF_SIZE);
    }
}

tty_buf **getTtyReceiveBuf()
{
    return tty_receive_buf;
}

tty_buf **getTtyTransmitBuf()
{
    return tty_transmit_buf;
}

int *getTerminalTransmitStatus()
{
    return terminal_transmit_status;
}

