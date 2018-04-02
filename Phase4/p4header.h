#ifndef P4HEADER_H
#define P4HEADER_H

#define DEBUG4 0

typedef struct proc4 proc4;
typedef struct proc4 *proc4Ptr;

struct proc4{
    int pid,
        //TODO Decide between mailboxes or semaphores
        (* startFunc) (char *);
    proc4Ptr nextProcPtr,
             childPtr,
             nextSiblingPtr,
             parentPtr;
};

#endif