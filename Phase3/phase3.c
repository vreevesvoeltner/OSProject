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

void nullsys3(USLOSS_Sysargs* sysArgs){
    USLOSS_Console("nullsys(): Invalid syscall %d. Terminating...\n", sysArgs->number);
    terminateReal(1);
}

void spawn(USLOSS_Sysargs* sysArgs){
    int (*func)(char *) = sysArgs->arg1,
        stack_size = (int)(long)sysArgs->arg3,
        priority = (int)(long)sysArgs->arg4,
        pid;
    char *arg = sysArgs->arg2,
         *name = sysArgs->arg5;
    
    pid = spawnReal(name, func, arg, stack_size, priority);
    
    setUserMode();
    sysArgs->arg1 = (void*)(long)pid;
    sysArgs->arg4 = (void*)(long)0;
}

int spawnReal(char *name, int (*func)(char *), char *arg, int stack_size, int priority) 
{
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
    if (newProc->mbox == -1){
        newProc->mbox = MboxCreate(0, 0);
    }
        
    
    if (newProc->parentPtr->childPtr == NULL){
        newProc->parentPtr->childPtr = newProc;
    }else{
        proc3Ptr temp = newProc->parentPtr->childPtr;
        while (temp->nextSiblingPtr != NULL){
            temp = temp->nextSiblingPtr;
        }
        temp->nextSiblingPtr = newProc;
    }
    
    MboxCondSend(newProc->mbox, 0, 0);
    
    return pid;
}

int spawnLaunch(char *sysArgs){
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

void wait(USLOSS_Sysargs* sysArgs){
    int status,
        pid;
        
    pid = waitReal(&status);
    
    sysArgs->arg1 = (void*)(long)pid;
    sysArgs->arg2 = (void*)(long)status;
    
    setUserMode();
}

int waitReal(int* status){
    return join(status);
}

void terminate(USLOSS_Sysargs* sysArgs){
    terminateReal((int)(long)sysArgs->arg1);
    setUserMode();
}

void terminateReal(int status){
    proc3Ptr current = &ProcTable[getpid() % MAXPROC],
             temp = current->childPtr;
    while (temp != NULL) {
        current->childPtr = temp->nextSiblingPtr;
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

void semCreate(USLOSS_Sysargs* sysArgs){
    int value = (int)(long)sysArgs->arg1,
        semID;
    
    if (semCount == MAXSEMS){
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
        USLOSS_Console("semCreateReal: in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    int mutex,
        mboxID;
    mySemPtr newSem = &SemTable[nextSem];
        
    mutex = MboxCreate(0, 0);
    mboxID = MboxCreate(initVal, 0); // Create semaphore with initial value
    
    MboxSend(mutex, NULL, 0);
    
    newSem->semID = nextSem;
    newSem->value = initVal;
    newSem->mutexID = mutex;
    newSem->mboxID = mboxID;
    
    semCount++;
    
    if (semCount < MAXSEMS){
        while (SemTable[nextSem].semID != -1){
            nextSem++;
        }
    }

    MboxReceive(mutex, NULL, 0);
    
    return newSem->semID;
}

void semP(USLOSS_Sysargs* sysArgs){

}

void semPReal(int id){

}

void semV(USLOSS_Sysargs* sysArgs){

}

void semVReal(int id){

}

void semFree(USLOSS_Sysargs* sysArgs){

}

int semFreeReal(int id){
    return -1;
}
void getTimeOfDay(USLOSS_Sysargs* sysArgs){

}

void cpuTime(USLOSS_Sysargs* sysArgs){

}

void getPID(USLOSS_Sysargs* sysArgs){

}

void setUserMode(){
    int r = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
}

void initSem(int id){
    mySemPtr sem = &SemTable[id % MAXSEMS];
    
    sem->semID = -1;
    sem->value = 0;
    sem->mutexID = -1;
    sem->mboxID = -1;
    sem->blocked = NULL;
}