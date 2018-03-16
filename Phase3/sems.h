#ifndef SEMS_H
#define SEMS_H
#include <stdlib.h>

#define DEBUG3 0

typedef struct proc3 proc3;
typedef struct proc3 *proc3Ptr;
//TODO: Make procStruct
struct proc3{
    int pid;
    int mbox;
    proc3Ptr nextProcPtr;
    proc3Ptr childPtr;
    proc3Ptr nextSiblingPtr;
    proc3Ptr parentPtr;
    int (* startFunc) (char *);
};

//TODO: Make Semaphore structs

#endif