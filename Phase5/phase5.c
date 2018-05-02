/*
 phase5.c
Students:
Veronica Reeves
Thai Pham

University of Arizona
Computer Science 452

Summary:
implement a virtual memory (VM) system that supports
demand paging. The USLOSS MMU is used to configure a region of virtual memory whose
contents are process-specific. Using the USLOSS MMU to implement the virtual memory system.
 The basic idea is to use the MMU to implement a single-level page table, so that each process 
 will have its own page able for the VM region and will therefore have its own view of what the
 VM region contains.
 
 */


#include <usloss.h>
#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <providedPrototypes.h>
#include <vm.h>
#include <string.h>

static int Pager(char *buf);
void printPageTable(int pid);

extern void mbox_create(USLOSS_Sysargs *args_ptr);
extern void mbox_release(USLOSS_Sysargs *args_ptr);
extern void mbox_send(USLOSS_Sysargs *args_ptr);
extern void mbox_receive(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive(USLOSS_Sysargs *args_ptr);

Process processes[MAXPROC];

FTEptr frameTable;

DTEptr diskTable;

int pagerPID[MAXPAGERS]; // pager pids

int clockhand,
    clockhandMbox;

void* vmRegion = NULL; // adress of the beginning of the VM

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
int faultMbox;
VmStats  vmStats;


static void FaultHandler(int type, void * offset);

static void vmInit(USLOSS_Sysargs *sysargs);
void *vmInitReal(int mappings, int pages, int frames, int pagers);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
void vmDestroyReal(void);
void setUserMode();
/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy; 

    // Initialize the phase 5 process table

    // Initialize other structures as needed

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void vmInit(USLOSS_Sysargs *sysargs){
    CheckMode();
    
    int mappings = (int)(long)sysargs->arg1,
        pages = (int)(long)sysargs->arg2,
        frames = (int)(long)sysargs->arg3,
        pagers = (int)(long)sysargs->arg4;
        
    sysargs->arg1 = vmInitReal(mappings, pages, frames, pagers);
    
    if ((int)(long)sysargs->arg1 < 0)
        sysargs->arg4 = sysargs->arg1;
    else
        sysargs->arg4 = (void*)(long)0;
    
    setUserMode();
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr){
   CheckMode();
   
   vmDestroyReal();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *vmInitReal(int mappings, int pages, int frames, int pagers){
   int status;
   int dummy;
   int i;

   CheckMode();
   
    if (vmRegion > 0){
        if (DEBUG5)
            USLOSS_Console("vmInitReal: vmRegion already initialized\n");
        return (void*)(long)-2;
    }
    
    if (mappings != pages){
        if (DEBUG5)
            USLOSS_Console("vmInitReal: Mappings and pages are not equal.\n");
        return (void*)(long)-1;
    }
    
    status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
    if (status != USLOSS_MMU_OK) {
        USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
        abort();
    }
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
    
    frameTable = malloc(frames * sizeof(FTE));
    for(i = 0; i < frames; i ++){
        frameTable[i].state = FUNUSED;
        frameTable[i].frame = 0;
        frameTable[i].page = 0;
        frameTable[i].pid = -1;      
        frameTable[i].next = NULL;
    }
    clockhand = 0;
    clockhandMbox = MboxCreate(1, 0);

   /*
    * Initialize page tables.
    */
    
    for (i = 0; i < MAXPROC; i++){
        processes[i].pid = -1;
        processes[i].numPages = pages;
        processes[i].pageTable = NULL;
        
        faults[i].pid = -1;
        faults[i].addr = NULL;
        faults[i].replyMbox = MboxCreate(1, sizeof(int));
    }

   /* 
    * Create the fault mailbox or semaphore
    */
    
    faultMbox =  MboxCreate(pagers, sizeof(FaultMsg));

   /*
    * Fork the pagers.
    */
    
    for(int i = 0; i < MAXPAGERS; i++){
        pagerPID[i] = -1;
        if (i < pagers)
            pagerPID[i] = fork1("Pager", Pager, NULL, 8*USLOSS_MIN_STACK, PAGER_PRIORITY);
    }
    
    int blocks;
    diskSizeReal(1, &dummy, &dummy, &blocks);
    blocks *= 2;

    diskTable = malloc(blocks * sizeof(DTE));
    for (i = 0; i < blocks; i++){
        diskTable[i].pid = -1;
        diskTable[i].page = -1;
        diskTable[i].track = i / 2;
        diskTable[i].sector = (i % 2) * USLOSS_MmuPageSize() / USLOSS_DISK_SECTOR_SIZE;
    }
   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   vmStats.diskBlocks = blocks;
   vmStats.freeFrames = frames;
   vmStats.freeDiskBlocks = blocks;
   vmStats.new = 0;
   /*
    * Initialize other vmStats fields.
    */

   vmRegion = USLOSS_MmuRegion(&dummy);
   return vmRegion;
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void vmDestroyReal(void){

   CheckMode();
   USLOSS_MmuDone();
   
    int i,
        status;
    FaultMsg dummy;
    
    if (vmRegion == NULL)
        return;
   /*
    * Kill the pagers here.
    */
    vmRegion = NULL;
    for (i = 0; i < MAXPAGERS; i++) {
        if (pagerPID[i] == -1)
            break;
        if (DEBUG5) 
            USLOSS_Console("vmDestroyReal: zapping pager %d, pid %d \n", i, pagerPID[i]);
        MboxSend(faultMbox, &dummy, sizeof(FaultMsg)); // wake up pager
        zap(pagerPID[i]);
        join(&status);
    }
    
    for (i = 0; i < MAXPROC; i++) {
        MboxRelease(faults[i].replyMbox);
    }

    MboxRelease(faultMbox);


   /* 
    * Print vm statistics.
    */
   PrintStats();

} /* vmDestroyReal */


/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int type /* MMU_INT */,
             void* offset  /* Offset within VM region */)
{
   int cause,
       frameNum = 0,
       pageNum = 0;
   FaultMsg* fmsg = &(faults[getpid() % MAXPROC]);
   ProcPtr current = &(processes[getpid() % MAXPROC]);

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */	
    pageNum = (int)(long)offset / USLOSS_MmuPageSize();
    fmsg->pid = getpid();
    fmsg->addr = offset;

    MboxSend(faultMbox, fmsg, sizeof(FaultMsg));
    MboxReceive(fmsg->replyMbox, &frameNum, sizeof(int));
    
} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
    FaultMsg msg;
    char buffer[USLOSS_MmuPageSize()]; // buffer to read and write from disk
	Process *currProc;
    int frame = 0,
        page = 0,
        i;
	
    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
		MboxReceive(faultMbox, &msg, sizeof(FaultMsg));
        if (isZapped())
            break;
        
		currProc =  &processes[msg.pid % MAXPROC];
		page = (int)(long)msg.addr / USLOSS_MmuPageSize();
        /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
		 /* Look for free frame */
         
        if (vmStats.freeFrames > 0) {
            for (frame = 0; frame < vmStats.frames; frame++) {
                if (frameTable[frame].state == FUNUSED) {
                    USLOSS_MmuMap(TAG, 0, frame, USLOSS_MMU_PROT_RW);
                    vmStats.freeFrames--; 
                    memset(vmRegion, 0, USLOSS_MmuPageSize());
                    break;
                }
            }
        }
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
		 
		 
         // Look for unreferenced and dirty
		for(i = 0; i < vmStats.frames; i++){
        
        }
		
		  // First time be used

        // unmap 
        USLOSS_MmuSetAccess(frame, 0); 
        USLOSS_MmuUnmap(0, 0); // unmap page
		
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
        
        currProc->pageTable[page].frame = frame;
        if (currProc->pageTable[page].state == UNUSED) {
            vmStats.new++; 
        }
        currProc->pageTable[page].state = INFRAME;
        
        frameTable[frame].state = FINUSE;
        frameTable[frame].pid = currProc->pid;
        frameTable[frame].page = page;
        USLOSS_MmuMap(TAG, page, frame, USLOSS_MMU_PROT_RW);
        MboxSend(msg.replyMbox, &frame, sizeof(int));
    }
    return 0;
} /* Pager */

/*    
setUserMode()
    Switch from kernel to user mode 
*/
void setUserMode(){
    int r = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
    if (DEBUG5) {
        USLOSS_Console("%d get way from warning unused variable\n", r);
    }
}
