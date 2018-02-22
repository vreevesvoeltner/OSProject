#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include "message.h"

extern int debugflag2;
int InterruptBoxTable[7];
	
/* an error method to handle invalid syscalls */
void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall. Halting...\n");
    USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int dev, void *arg)
{
    static int count = 0;
    int status;
    if (DEBUG2 && debugflag2)
        USLOSS_Console("clockHandler2(): called\n");
      
    disableInterrupts();
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    if (dev != USLOSS_CLOCK_DEV) {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("clockHandler2(): called by other device, returning\n");
        return;
    }
    count++;
    if (count == 5) {
        if(USLOSS_DeviceInput(dev, 0, &status) == USLOSS_DEV_INVALID){
            if (DEBUG2 && debugflag2)
                USLOSS_Console("clockHandler2(): invalid entry, returning\n");
            return;
        }
        MboxCondSend(InterruptBoxTable[CLOCK_MBOX], &status, sizeof(int));
        count = 0;
    }

    timeSlice();
    enableInterrupts();
} /* clockHandler */


void diskHandler(int dev, void *arg)
{
    int status;
    long offset = (long)arg;
    
    if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): called\n");
    
    disableInterrupts();
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    if (dev != USLOSS_DISK_DEV) {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("diskHandler(): called by other device, returning\n");
        return;
    }
    
    if(USLOSS_DeviceInput(dev, offset, &status) == USLOSS_DEV_INVALID){
        if (DEBUG2 && debugflag2)
            USLOSS_Console("diskHandler(): invalid entry, returning\n");
        return;
    }
    MboxCondSend(InterruptBoxTable[DISK_MBOX + offset], &status, sizeof(int));
    
    enableInterrupts();
} /* diskHandler */


void termHandler(int dev, void *arg)
{
    int status;
    long offset = (long)arg;
    
    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): called\n");
    
    disableInterrupts();
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    if (dev != USLOSS_TERM_DEV) {
        if (DEBUG2 && debugflag2)
            USLOSS_Console("termHandler(): called by other device, returning\n");
        return;
    }
    
    if(USLOSS_DeviceInput(dev, offset, &status) == USLOSS_DEV_INVALID){
        if (DEBUG2 && debugflag2)
            USLOSS_Console("termHandler(): invalid entry, returning\n");
        return;
    }
    MboxCondSend(InterruptBoxTable[TERMINAL_MBOX + offset], &status, sizeof(int));
    enableInterrupts();

} /* termHandler */


void syscallHandler(int dev, void *arg)
{
    USLOSS_Sysargs *sysArgs = (USLOSS_Sysargs*) arg;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): called\n");
    
    disableInterrupts();
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    if (dev != USLOSS_SYSCALL_INT) {
        if (DEBUG2 && debugflag2) 
            USLOSS_Console("sysCallHandler(): called by other device, returning\n");
        return;
    }


    if (sysArgs->number < 0 || sysArgs->number >= MAXSYSCALLS) {
        USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n", sysArgs->number);
        USLOSS_Halt(1);
    }

    nullsys(sysArgs);
    enableInterrupts();
} /* syscallHandler */
