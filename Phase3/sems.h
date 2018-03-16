#ifndef SEMS_H
#define SEMS_H
#include <stdlib.h>

#define DEBUG3 0

typedef struct proc3 proc3;
typedef struct proc3 *proc3Ptr;
typedef struct mySem mySem;
typedef struct mySem* mySemPtr; 

struct proc3{
    int pid,
        mbox,
        (* startFunc) (char *);
    proc3Ptr nextProcPtr,
             childPtr,
             nextSiblingPtr,
             parentPtr;
};
//TODO: Make Semaphore structs
struct mySem{
    int semID,
        value,
        mutexID,
        mboxID;
    proc3Ptr blocked;
};

#endif