#ifndef P4HEADER_H
#define P4HEADER_H

#define DEBUG4 0

#include <string.h>

typedef struct proc4 proc4;
typedef struct proc4 *proc4Ptr;

struct proc4{
    int pid,
        track,
        sleepTime,  // Time when the process should wake up
        waitSem;    // Use to block processes from continuing
    proc4Ptr nextProcPtr,
             nextDiskPtr,
             nextSleepPtr; // Next process in the sleep list
    USLOSS_DeviceRequest request;
};

#endif