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
    
    static int count = 0;
    count++;
    if (count == 5) {
      int status;
      USLOSS_DeviceInput(dev, 0, &status); // get the status
      MboxCondSend(InterruptBoxTable[CLOCK_MBOX], &status, sizeof(int));
      count = 0;
    }

    timeSlice();
    enableInterrupts();
} /* clockHandler */


void diskHandler(int dev, void *arg)
{

   if (DEBUG2 && debugflag2)
      USLOSS_Console("diskHandler(): called\n");


} /* diskHandler */


void termHandler(int dev, void *arg)
{

   if (DEBUG2 && debugflag2)
      USLOSS_Console("termHandler(): called\n");


} /* termHandler */


void syscallHandler(int dev, void *arg)
{

   if (DEBUG2 && debugflag2)
      USLOSS_Console("syscallHandler(): called\n");


} /* syscallHandler */
