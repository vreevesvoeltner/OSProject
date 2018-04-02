/*
 *  File:  libuser.c
 *  Students:
 *    Veronica Reeves
 *    Thai Pham
 *  University of Arizona
 *  Computer Science 452
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <libuser.h>

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

/*
 *  Routine:  Sleep
 *
 *  Description: 
 *
 *  Arguments:      int seconds --
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Sleep(int seconds){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_SLEEP;
    
    sysArg.arg1 = (void*)(long)seconds;
    
    USLOSS_Syscall(&sysArg);
    
    return (int)(long)sysArg.arg4;
}

/*
 *  Routine:  DiskRead
 *
 *  Description: 
 *
 *  Arguments:      void* dbuff --
 *                  int unit    --
 *                  int track   --
 *                  int first   --
 *                  int sectors --
 *                  int* status --
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void*)(long)sectors;
    sysArg.arg3 = (void*)(long)track;
    sysArg.arg4 = (void*)(long)first;
    sysArg.arg5 = (void*)(long)unit;
    
    USLOSS_Syscall(&sysArg);
    
    *status = (int)(long)sysArg.arg1;

    return (int)(long)sysArg.arg4;
}

/*
 *  Routine:  DiskWrite
 *
 *  Description: 
 *
 *  Arguments:      void* dbuff --
 *                  int unit    --
 *                  int track   --
 *                  int first   --
 *                  int sectors --
 *                  int* status --
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_DISKWRITE;
    
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void*)(long)sectors;
    sysArg.arg3 = (void*)(long)track;
    sysArg.arg4 = (void*)(long)first;
    sysArg.arg5 = (void*)(long)unit;
    
    USLOSS_Syscall(&sysArg);
    
    *status = (int)(long)sysArg.arg1;

    return (int)(long)sysArg.arg4;
}

 /*
 *  Routine:  DiskSize
 *
 *  Description: 
 *
 *  Arguments:      int unit    --
 *                  int* sector --
 *                  int* track  --
 *                  int* disk   -- 
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int DiskSize(int unit, int *sector, int *track, int *disk){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    
    sysArg.arg1 = (void*)(long)unit;
    
    USLOSS_Syscall(&sysArg);
    
    *sector = (int)(long)sysArg.arg1;
    *track = (int)(long)sysArg.arg2;
    *disk = (int)(long)sysArg.arg3;
    
    return (int)(long)sysArg.arg4;
} 

/*
 *  Routine:  TermRead
 *
 *  Description: 
 *
 *  Arguments:      char* buff  --
 *                  int bsize   --
 *                  int unit_id --
 *                  int* nread  --
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int TermRead(char *buff, int bsize, int unit_id, int *nread){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    
    sysArg.arg1 = buff;
    sysArg.arg2 = (void*)(long)bsize;
    sysArg.arg3 = (void*)(long)unit_id;
    
    USLOSS_Syscall(&sysArg);
    
    *nread = (int)(long)sysArg.arg2;
    
    return (int)(long)sysArg.arg4;
}

/*
 *  Routine:  TermWrite
 *
 *  Description: 
 *
 *  Arguments:      char* buff  --
 *                  int bsize   --
 *                  int unit_id --
 *                  int* nwrite --
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int TermWrite(char *buff, int bsize, int unit_id, int *nwrite){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    
    sysArg.arg1 = buff;
    sysArg.arg2 = (void*)(long)bsize;
    sysArg.arg3 = (void*)(long)unit_id;
    
    USLOSS_Syscall(&sysArg);
    
    *nwrite = (int)(long)sysArg.arg2;
    
    return (int)(long)sysArg.arg4;
}