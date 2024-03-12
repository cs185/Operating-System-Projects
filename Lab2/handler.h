#ifndef YALNIX_HANDLER_H
#define YALNIX_HANDLER_H
#include <comp421/hardware.h>
#include <comp421/yalnix.h>
// this file stores the interrupt handler functions

typedef void (*InterruptHandler)(ExceptionInfo *);

// the hander for system calles (trap kernel)
void onTrapKernel(ExceptionInfo *info);

// the handler for clock interrupt
void onTrapClock(ExceptionInfo *info);

// the handler for illegal instruction exception
void onTrapIllegal(ExceptionInfo *info);

// the handler for memory interrupt on stack
void onTrapMemory(ExceptionInfo *info);

// the handler for math exception
void onTrapMath(ExceptionInfo *info);

// trap for terminal receive
void onTrapTTYReceive(ExceptionInfo *info);

// trap for terminal transmit
void onTrapTTYTransmit(ExceptionInfo *info);

#endif // YALNIX_HANDLER_H
