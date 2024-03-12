#ifndef YALNIX_SWICH_H
#define YALNIX_SWICH_H

#include <comp421/hardware.h>
#include <comp421/yalnix.h>

#define AVOID_UNUSED_WARNING(x) (void)x

// this files stores the switch function for context switch

// this function really do the context switch
SavedContext *NormalSwitch(SavedContext *ctxp, void *p1, void *p2);

// the switch function to copy all pages and continue the execution after switching
SavedContext *ForkSwitch(SavedContext *ctxp, void *p1, void *p2);

// the switch function to Exit the first process and switch to the next process
SavedContext *ExitSwitch(SavedContext *ctxp, void *p1, void *p2);

#endif // YALNIX_SWICH_H
