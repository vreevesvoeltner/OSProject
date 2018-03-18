/* ------------------------------------------------------------------------
phase3.c +
Students:
Veronica Reeves
Thai Pham

University of Arizona
Computer Science 452

Summary:
designing and implementing a support level that creates user mode processes
and then provides system services to the user processes. All of these
services will be requested by user programs through the syscall interface. 
Many of the services that the support level provides are almost completely 
implemented by the kernel primitives. In addition, in phase 4, the support 
level will be expanded to contain system driver processes to handle
the pseudo-clock, terminal input, and disk I/O. These system processes run
in kernel mode and with interrupts enabled.
In phases 1 and 2, when an error occurred, the kernel
reported the error and shutdown USLOSS. This simulates an operating system
crash. Starting with phase 3 and user-level processes, the operating system
should not crash when an error is made by a user-level process. Instead, 
the offending process will terminate
------------------------------------------------------------------------ */



#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <libuser.h>
#include <sems.h>

void nullsys3(USLOSS_Sysargs *);
void spawn(USLOSS_Sysargs *);
int spawnReal(char *, int(*)(char *), char *, int, int);
int spawnLaunch(char*);
void wait(USLOSS_Sysargs *);
int waitReal(int *);
void terminate(USLOSS_Sysargs *);
void terminateReal(int);
void semCreate(USLOSS_Sysargs *);
int semCreateReal(int);
void semP(USLOSS_Sysargs *);
void semPReal(int);
void semV(USLOSS_Sysargs *);
void semVReal(int);
void semFree(USLOSS_Sysargs *);
int semFreeReal(int); 
void getTimeOfDay(USLOSS_Sysargs *);
void cpuTime(USLOSS_Sysargs *);
void getPID(USLOSS_Sysargs *);
void setUserMode();
void initSem(int);

proc3 ProcTable[MAXPROC];

int semCount;

int nextSem;

mySem SemTable[MAXSEMS];

int start2(char *arg)
{
    int pid;
    int status;
    /*
     * Check kernel mode here.
     */

    /*
     * Data structure initialization as needed...
     */
     for (int i = 0; i < MAXPROC; i++){
        ProcTable[i].mbox = -1;
     }
     
     semCount = 0;
     
     nextSem = 0;
     
     for (int i = 0; i < MAXSEMS; i++){
        initSem(i);
     }
     
     for (int i = 0; i < USLOSS_MAX_SYSCALLS; i++){
        systemCallVec[i] = nullsys3;
     }
     
     systemCallVec[SYS_SPAWN] = spawn;
     systemCallVec[SYS_WAIT] = wait;
     systemCallVec[SYS_TERMINATE] = terminate;
     systemCallVec[SYS_SEMCREATE] = semCreate;
     systemCallVec[SYS_SEMP] = semP;
     systemCallVec[SYS_SEMV] = semV;
     systemCallVec[SYS_SEMFREE] = semFree;
     systemCallVec[SYS_GETTIMEOFDAY] = getTimeOfDay;
     systemCallVec[SYS_CPUTIME] = cpuTime;
     systemCallVec[SYS_GETPID] = getPID ;


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler (via the systemCallVec array);s
     * spawnReal is the function that contains the implementation and is
     * called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, start2 calls spawnReal(), since start2 is in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
    return pid;
} /* start2 */

/*
nullsys3() 
Assign every element of systemCallVec, which is visible in the provided phase2 libraries. 
The nullsys3 function will call terminateReal to terminate the process, 
rather than shutting down the simulation.
*/
void nullsys3(USLOSS_Sysargs* sysArgs){
    USLOSS_Console("nullsys(): Invalid syscall %d. Terminating...\n", sysArgs->number);
    terminateReal(1);
}


/*
Spawn() 
It invokes USLOSS_Syscall()
The system call handler calls a function named spawn() 
.Note lower case -- that extracts the arguments from the sysargs pointer, and 
checks them for possible errors.  This function then calls spawnReal().
*/
void spawn(USLOSS_Sysargs* sysArgs){
    int (*func)(char *) = sysArgs->arg1,
        stack_size = (int)(long)sysArgs->arg3,
        priority = (int)(long)sysArgs->arg4,
        pid;
    char *arg = sysArgs->arg2,
         *name = sysArgs->arg5;
    
    pid = spawnReal(name, func, arg, stack_size, priority);
    
	// terminate the current process if it is zapped 
    if (isZapped())
        terminateReal(1); 
	
    setUserMode();
    sysArgs->arg1 = (void*)(long)pid;
    sysArgs->arg4 = (void*)(long)0;
}

/*
* spawnReal() will create the process by using a call to fork1 to
 create a process executing the code in spawnLaunch().  spawnReal()
 and spawnLaunch() then coordinate the completion of the phase 3
 process table entries needed for the new process.  spawnReal() will
 return to the original caller of Spawn. 
*/
int spawnReal(char *name, int (*func)(char *), char *arg, int stack_size, int priority){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("spawnReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    int pid;
    proc3Ptr newProc;
    
    pid = fork1(name, spawnLaunch, arg, stack_size, priority);
    
    if (pid < 0){
        return -1;
    }
    
    newProc = &ProcTable[pid % MAXPROC];
    newProc->pid = pid;
    newProc->nextProcPtr = NULL;
    newProc->nextSiblingPtr = NULL;
    newProc->parentPtr = &ProcTable[getpid() % MAXPROC];
    newProc->startFunc = func;
    
    if (newProc->parentPtr->childPtr == NULL){
        newProc->parentPtr->childPtr = newProc;
    }else{
        proc3Ptr temp = newProc->parentPtr->childPtr;
        while (temp->nextSiblingPtr != NULL){
            temp = temp->nextSiblingPtr;
        }
        temp->nextSiblingPtr = newProc;
    }
    
    if (newProc->mbox == -1){
        newProc->mbox = MboxCreate(0, 0);
    }
    
    MboxCondSend(newProc->mbox, 0, 0);
    
    return pid;
}

/*
spawnLaunch() 
Begin executing the function passed to Spawn. spawnLaunch() will
need to switch to user-mode before allowing user code to execute.
spawnReal() will return to spawn(), which will put the return
values back into the sysargs pointer, switch to user-mode, and 
return to the user code that called Spawn.
*/
int spawnLaunch(char *sysArgs){
    // terminate the current process if it is zapped 
    if (isZapped())
        terminateReal(1); 
	
	int procSlot = getpid() % MAXPROC,
        status;
    
    if (ProcTable[procSlot].mbox == -1){
        ProcTable[procSlot].mbox = MboxCreate(0, 0);
        MboxReceive(ProcTable[procSlot].mbox, 0, 0);
    }
    
    setUserMode();
    
    status = ProcTable[procSlot].startFunc(sysArgs);
    
    Terminate(status);
    
    return 0;
}


/*
wait()
Getting PID and the status of termination from calling  waitReal(). 
Then it save those values to arg1 and arg2 
*/
void wait(USLOSS_Sysargs* sysArgs){
    int status,
        pid;
        
    pid = waitReal(&status);
    
    sysArgs->arg1 = (void*)(long)pid;
    sysArgs->arg2 = (void*)(long)status;
    
    setUserMode();
}

/*
waitReal() 
Return the join status and join() return value which are:
-2: the process does not have any children who have not already been joined.
-1: the process was zapped while waiting for a child to quit.
>0: the PID of the child that quit. 
This operation synchronizes termination of a child with its parent
*/
int waitReal(int* status){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("waitReal: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    return join(status);
}


/*
terminate() 
Passing the process PID to terminateReal() to terminate the process 
then switch to user mode 
*/
void terminate(USLOSS_Sysargs* sysArgs){
    terminateReal((int)(long)sysArgs->arg1);
    setUserMode();
}

/*
Terminate the current process and all its children 
*/
void terminateReal(int status){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("terminateReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    proc3Ptr current = &ProcTable[getpid() % MAXPROC],
             temp = current->childPtr;
    while (temp != NULL) {
        zap(temp->pid);
        temp = current->childPtr;
    }
    
    if (current->parentPtr != NULL){
        temp = current->parentPtr->childPtr;
        if (temp == current){
            current->parentPtr->childPtr = current->nextSiblingPtr;
        }else{
            while (temp->nextSiblingPtr != current){
                temp = temp->nextSiblingPtr;
            }
            temp->nextSiblingPtr = current->nextSiblingPtr;
        }
    }
    
    quit(status);
}

/*
semCreate ()
	Create a new semaphore. 
	and set value for arg1 and arg4 
*/
void semCreate(USLOSS_Sysargs* sysArgs){
    int value = (int)(long)sysArgs->arg1,
        semID;
    
    if (semCount == MAXSEMS || value < 0){
        sysArgs->arg4 = (void*)(long)-1;
    }else{
        semID = semCreateReal(value);
        
        sysArgs->arg1 = (void*)(long)semID;
        sysArgs->arg4 = (void*)(long)0;
    }
    
    setUserMode();
}

int semCreateReal(int initVal){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("semCreateReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    int mutex,
        mboxID,
        block;
    mySemPtr newSem = &SemTable[nextSem];
        
    mutex = MboxCreate(1, 0);
    mboxID = MboxCreate(1, sizeof(int)); // Create semaphore with initial value
    block = MboxCreate(0, 0);
    
    newSem->semID = nextSem;
    newSem->value = initVal;
    newSem->mutexID = mutex;
    newSem->mboxID = mboxID;
    newSem->block = block;
    
    semCount++;
    
    if (semCount < MAXSEMS){
        while (SemTable[nextSem].semID != -1){
            nextSem++;
        }
    }
    
    MboxSend(mutex, NULL, 0);
    MboxSend(mboxID, (void*)&initVal, sizeof(int));
    
    return newSem->semID;
}

/*
semP ()
	Perform the P operation on semaphore. 
	This will call semaphore to perform the operation 
*/
void semP(USLOSS_Sysargs* sysArgs){
    int semID = (int)(long)sysArgs->arg1,
        result = 0;
    
    if (SemTable[semID % MAXSEMS].semID == -1){
        result = -1;
    }else{
        semPReal(semID);
    }
    
    sysArgs->arg4 = (void*)(long)result;
    setUserMode();
}


void semPReal(int id){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("semPReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    mySemPtr sem = &SemTable[id % MAXSEMS];
    proc3Ptr current = &ProcTable[getpid() % MAXPROC];
    int value,
        result;
    
    MboxReceive(sem->mutexID, NULL, 0);
    result = MboxReceive(sem->mboxID, &value, sizeof(int));
    if (value == 0){
        if (sem->blocked == NULL){
            sem->blocked = current;
        }else{
            proc3Ptr temp = sem->blocked;
            while (temp->nextProcPtr != NULL){
                temp = temp->nextProcPtr;
            }
            temp->nextProcPtr = current;
        }
        MboxSend(sem->mboxID, &value, sizeof(int));
        MboxSend(sem->mutexID, NULL, 0);
        /*If issue most likely because of this*/
        MboxReceive(sem->block, NULL, 0); // block self
        
        /*Check if semaphore was freed*/
        if (SemTable[id % MAXSEMS].semID == -1){
            terminateReal(1);
        }
        
        MboxReceive(sem->mutexID, NULL, 0); // Get mutex after we've unblocked
        result = MboxReceive(sem->mboxID, &value, sizeof(int));
    }
    value--;
    MboxSend(sem->mboxID, &value, sizeof(int));
    
    if (result < 0){
        USLOSS_Console("semP(): Receive failed");
    }
    MboxSend(sem->mutexID, NULL, 0);
}

void semV(USLOSS_Sysargs* sysArgs){
    int semID = (int)(long)sysArgs->arg1,
        result = 0;
    
    if (SemTable[semID % MAXSEMS].semID == -1){
        result = -1;
    }else{
        semVReal(semID);
    }
    
    sysArgs->arg4 = (void*)(long)result;
    setUserMode();
}

void semVReal(int id){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("semVReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    mySemPtr sem = &SemTable[id % MAXSEMS];
    proc3Ptr blockedProc;
    int value;
    
    MboxReceive(sem->mutexID, NULL, 0);
    MboxReceive(sem->mboxID, &value, sizeof(int));
    value++;
    
    if (sem->blocked != NULL){
        blockedProc = sem->blocked;
        sem->blocked = blockedProc->nextProcPtr;
        blockedProc->nextProcPtr = NULL;
        
        MboxSend(sem->block, NULL, 0);
    }
    MboxSend(sem->mboxID, &value, sizeof(int));
    MboxSend(sem->mutexID, NULL, 0);
}

void semFree(USLOSS_Sysargs* sysArgs){
    int semID = (int)(long)sysArgs->arg1;
    
    sysArgs->arg4 = (void*)(long)semFreeReal(semID);
    
    setUserMode();
}

int semFreeReal(int id){
    mySemPtr sem = &SemTable[id % MAXSEMS];
    proc3Ptr blockedProc = sem->blocked,
             temp;
    int mutex,
        mboxID,
        block,
        result = 0;

    if (sem->semID == -1){
        return -1;
    }
    mutex = sem->mutexID;
    mboxID = sem->mboxID;
    block = sem->block;
    
    initSem(id);
    
    while (blockedProc != NULL){
        result = 1;
        MboxSend(block, NULL, 0);
        temp = blockedProc->nextProcPtr;
        blockedProc->nextProcPtr = NULL;
        blockedProc = temp;
    }
    
    MboxRelease(mutex);
    MboxRelease(mboxID);
    MboxRelease(block);
    
    semCount--;
    
    return result;
}
/*
getTimeOfDay()
Set arg1 to current system time
*/
void getTimeOfDay(USLOSS_Sysargs* sysArgs){
	int tofd;
	int r = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &tofd);
	sysArgs->arg1 = (void*)(long)tofd;
	if (DEBUG3) {
		USLOSS_Console("%d get way from warning unused variable\n", r);
	}
	setUserMode();
}

/*
cpuTime ()
set arg1 to the CPU time (in milliseconds) used by the current process.
This means that the kernel must record the amount of processor time used 
by each process. Do not use the clock interrupt to measure CPU time as it 
is too coarse-grained; use USLOSS_DeviceInput to get the current time 
from the USLOSS clock .
*/
void cpuTime(USLOSS_Sysargs* sysArgs){
	int rt = readtime();
	//USLOSS_Console("--------------->>CPU time: %d \n", rt);
	sysArgs->arg4 = (void*)(long)rt; // readtime() is function from Phase1
	setUserMode();
}

/*
set arg1 to the PID of the calling process
*/
void getPID(USLOSS_Sysargs* sysArgs){
	int ip = getpid();
	sysArgs->arg1 = (void*)(long)ip; // getpid() (Phase1`s function) 
	setUserMode();
}

void setUserMode(){
    int r = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
    if (DEBUG3) {
        USLOSS_Console("%d get way from warning unused variable\n", r);
    }
}

void initSem(int id){
    mySemPtr sem = &SemTable[id % MAXSEMS];
    
    sem->semID = -1;
    sem->value = 0;
    sem->mutexID = -1;
    sem->mboxID = -1;
    sem->block = -1;
    sem->blocked = NULL;
}