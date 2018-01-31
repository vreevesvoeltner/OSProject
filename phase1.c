/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
void illegalInstructionHandler(int dev, void *arg);

int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
void clckHandler(int, void*);
void disableInterrupts();
void enableInterrupts();
void initProcTable();
void initProc(int);
void readyUp(procPtr, int);


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList[6];

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

// number of processes
int numProcs = 0;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result, /* value returned by call to fork1() */
        i;

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    for (i = 0; i < 6; i++){
        ReadyList[i] = NULL;
    }
    
    initProcTable();
    Current = &ProcTable[0];

    // Initialize the illegalInstruction interrupt handler
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = illegalInstructionHandler;

    // Initialize the clock interrupt handler

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    disableInterrupts();
    int procSlot = -1;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("fork1(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK){
        USLOSS_Console("fork1(): Stack size too small.");
        return -2;
    }
    // Is there room in the process table? What is the next PID?
    if (numProcs == MAXPROC){
        USLOSS_Console("fork1(): No room in table.");
        return -1;
    }
    if (name == NULL){
        USLOSS_Console("fork1(): Name is NULL.");
        return -1;
    }
    if (startFunc == NULL){
        USLOSS_Console("fork1(): Start function is NULL.");
        return -1;
    }
    if (priority < 1 || (priority > 5 && startFunc != sentinel) || priority > 6){
        USLOSS_Console("fork1(): Priority out of range.");
        return -1;
    }
    
    // Find empty slot in table
    do{
        procSlot = nextPid % MAXPROC;
        nextPid++;
    }while (ProcTable[procSlot].status != EMPTYSTATUS);
    

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);
    ProcTable[procSlot].pid = nextPid - 1;
    ProcTable[procSlot].priority = priority;
    readyUp(&ProcTable[procSlot], priority - 1);
    if (Current->pid > 1)
        ProcTable[procSlot].parentProcPtr = Current;
    ProcTable[procSlot].stack = (char*)malloc(stacksize);
    ProcTable[procSlot].stackSize = stacksize;

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)

    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...
    if (Current->pid > 1){
        if (Current->childProcPtr == NULL){
            Current->childProcPtr = &ProcTable[procSlot];
        }else{
            procPtr temp = Current->childProcPtr;
            while (temp->nextSiblingPtr != NULL){
                temp = temp->nextSiblingPtr;
            }
            temp->nextSiblingPtr = &ProcTable[procSlot];
        }
    }
    numProcs++;
    
    if (ProcTable[procSlot].pid != 1){
        dispatcher();
    }

    return ProcTable[procSlot].pid;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
    enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("join(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
        
    if (Current->childProcPtr == NULL){
        USLOSS_Console("join(): No children to join.");
        return -2;
    }
    
    int kpid = -1;
    if (Current->quitChildPtr == NULL){
        Current->status = JOINBLOCKSTATUS;
        dispatcher();
    }else{
        procPtr kid = Current->quitChildPtr;
        kpid = kid->pid;
        Current->quitChildPtr = kid->nextProcPtr;
        *status = kid->quitStatus;
        initProc(kpid % MAXPROC);
    }
    return kpid;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
printf("In quit\n");
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("fork1(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    procPtr parent = Current->parentProcPtr,
            temp;

    Current->status = QUITSTATUS;
    Current->quitStatus = status;

    if (parent->quitChildPtr == NULL){
        parent->quitChildPtr = Current;
    }else{
        temp = parent->quitChildPtr;
        while (temp->nextProcPtr != NULL){
            temp = temp->nextProcPtr;
        }
        temp->nextProcPtr = Current;
    }

    Current->nextProcPtr = NULL;
    p1_quit(Current->pid);

    if (parent->childProcPtr == Current){
        parent->childProcPtr = Current->nextSiblingPtr;
    }else{
        temp = parent->childProcPtr;
        while (temp->nextSiblingPtr != Current){
            temp = temp->nextSiblingPtr;
        }
        temp->nextSiblingPtr = Current->nextSiblingPtr;
    }

    if (parent->status == JOINBLOCKSTATUS){
        readyUp(parent, parent->priority - 1);
    }
    dispatcher();
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    // Check if in kernel mode
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("dispatcher(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    procPtr nextProcess = NULL;
    int i;
    
    // Find next process to run
    for (i = 0; i < 6; i++){
        if (ReadyList[i] != NULL){
            nextProcess = ReadyList[i];
            ReadyList[i] = ReadyList[i]->nextProcPtr;
            break;
        }
    }

    //TODO: Check if next is same as Current
    if (Current->status == RUNSTATUS){
        //TODO: check clock
        readyUp(Current, Current->priority - 1);
    }
    p1_switch(Current->pid, nextProcess->pid);
    enableInterrupts();	

    procPtr tempProc = Current;
    Current = nextProcess;
    if (Current->status == EMPTYSTATUS || Current->status == QUITSTATUS)
        USLOSS_ContextSwitch(NULL, &Current->state	);
    else
        USLOSS_ContextSwitch(&tempProc->state, &Current->state);
        
    Current = nextProcess;
    Current->status = RUNSTATUS;
    
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
static void checkDeadlock()
{
} /* checkDeadlock */


/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    if ((0x1 & USLOSS_PsrGet()) == 0){
        USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    USLOSS_PsrSet(USLOSS_PsrGet() ^ (USLOSS_PsrGet() & 0x2));

} /* disableInterrupts */

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void enableInterrupts()
{
    // turn the interrupts IN iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0){
        USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);	

} /* enableInterrupts */

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void clockHandler(int dev, void *arg){

}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void illegalInstructionHandler(int dev, void *arg)
{
    if (DEBUG && debugflag)
        USLOSS_Console("illegalInstructionHandler() called\n");
} /* illegalInstructionHandler */

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void initProcTable(){
    int i;
    for (i = 0; i < MAXPROC; i++){
        initProc(i);
    }
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void initProc(int loc){
    ProcTable[loc].nextProcPtr = NULL;
    ProcTable[loc].parentProcPtr = NULL;
    ProcTable[loc].childProcPtr = NULL;
    ProcTable[loc].nextSiblingPtr = NULL;
    ProcTable[loc].quitChildPtr = NULL;
    ProcTable[loc].pid = 0;
    ProcTable[loc].stack = NULL;
    ProcTable[loc].status = EMPTYSTATUS;
    ProcTable[loc].quitStatus = 0;
}

/* ------------------------------------------------------------------------
   Name - 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects -  
   ----------------------------------------------------------------------- */
void readyUp(procPtr proc, int loc){
    if (ReadyList[loc] == NULL){
        ReadyList[loc] = proc;
        proc->status = READYSTATUS;
    }else{
        procPtr temp = ReadyList[loc];
        while (temp->nextProcPtr != NULL){
            temp = temp->nextProcPtr;
        }
        temp->nextProcPtr = proc;
        proc->status = READYSTATUS;
    }
}
