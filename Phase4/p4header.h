#ifndef P4HEADER_H
#define P4HEADER_H

#define DEBUG4 0

typedef struct proc4 proc4;
typedef struct proc4 *proc4Ptr;
typedef struct diskList diskList;

struct diskList {
    proc4Ptr head;
    proc4Ptr tail;
    int size;
};

struct proc4{
    int pid,
        unit,
        firstSector,
        sectors,
        track,
        sleepTime,  // Time when the process should wake up
        waitSem;    // Use to block processes from continuing
    proc4Ptr nextProcPtr,
             nextDiskPtr,
             prevDiskPtr, // TP
             nextSleepPtr; // Next process in the sleep list
    USLOSS_DeviceRequest request;
};

#endif