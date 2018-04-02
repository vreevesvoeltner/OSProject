#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <providedPrototypes.h>
#include <p4header.h>
#include <libuser.h>
#include <stdio.h>
#include <stdlib.h> /* needed for atoi() */

int  semRunning;

proc4 ProcTable[MAXPROC];

int ClockDriver(char *);
int DiskDriver(char *);
int TermDriver(char *);
int TermReader(char *);
int TermWriter(char *);

void sleep(USLOSS_Sysargs*);
int sleepReal(int);
void diskRead(USLOSS_Sysargs*);
int diskReadReal(int, int, int, int, void *);
void diskWrite(USLOSS_Sysargs*);
int diskWriteReal(int, int, int, int, void *);
void diskSize(USLOSS_Sysargs*);
int diskSizeReal(int, int*, int*, int*);
void termRead(USLOSS_Sysargs*);
int termReadReal(int, int, char *);
void termWrite(USLOSS_Sysargs*);
int termWriteReal(int, int, char *);
void initProc(int);

//int diskReadOrWriteReal(int, int, int, int, void *, int);

void start3(void)
{
    char    name[128],
            termbuf[10],
            filename[50];
    int     i,
            clockPID,
            diskPID[USLOSS_DISK_UNITS],
            termPID[USLOSS_TERM_UNITS][3],
            pid,
            status;
    /*
     * Check kernel mode here.
     */
     
     
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
    systemCallVec[SYS_TERMREAD] = termRead;
    systemCallVec[SYS_TERMWRITE] = termWrite;

    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
     
     for (i = 0; i < MAXPROC; i++){
        initProc(i);
     }
     
    semRunning = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
    USLOSS_Console("start3(): Can't create clock driver\n");
    USLOSS_Halt(1);
    }
    ProcTable[clockPID % MAXPROC].pid = clockPID;
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "semRunning" once it is running.
     */

    sempReal(semRunning);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        //TODO: Figure out what is needed for Disk buffer
        diskPID[i] = fork1("Disk Driver", DiskDriver, NULL, USLOSS_MIN_STACK, 2);
        if (diskPID[i] < 0) {
            USLOSS_Console("start3(): Can't create disk driver\n");
            USLOSS_Halt(1);
        }
        ProcTable[diskPID[i] % MAXPROC].pid = diskPID[i];
        sempReal(semRunning);
        
        //TODO: Get size of Disk
    }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */
     for (i = 0; i < USLOSS_TERM_UNITS; i++){
        termPID[i][0] = fork1(name, TermDriver, termbuf, USLOSS_MIN_STACK, 2);
        if (termPID[i][0] < 0) {
            USLOSS_Console("start3(): Can't create term driver\n");
            USLOSS_Halt(1);
        }
        ProcTable[termPID[i][0] % MAXPROC].pid = termPID[i][0];
        sempReal(semRunning);
        
        termPID[i][1] = fork1(name, TermReader, termbuf, USLOSS_MIN_STACK, 2);
        if (termPID[i][1] < 0) {
            USLOSS_Console("start3(): Can't create term reader\n");
            USLOSS_Halt(1);
        }
        ProcTable[termPID[i][1] % MAXPROC].pid = termPID[i][1];
        sempReal(semRunning);
        
        termPID[i][2] = fork1(name, TermWriter, termbuf, USLOSS_MIN_STACK, 2);
        if (termPID[i][2] < 0) {
            USLOSS_Console("start3(): Can't create term writer\n");
            USLOSS_Halt(1);
        }
        ProcTable[termPID[i][2] % MAXPROC].pid = termPID[i][2];
        sempReal(semRunning);
     }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver
    join(&status);
    
    for(i = 0; i < USLOSS_DISK_UNITS; i++){
        //TODO: Get mutex
        zap(diskPID[i]);
        join(&status);
    }
    
    for (i = 0; i < USLOSS_TERM_UNITS; i++){
        FILE *f;
        
        //TODO: Get mutex
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)(long) USLOSS_TERM_CTRL_RECV_INT(0));
        sprintf(filename, "term%d.in", i);
        f = fopen(filename, "a+");
        fprintf(f, "last line\n");
        fflush(f);
        fclose(f);
        
        zap(termPID[i][0]);
        join(&status);
        zap(termPID[i][1]);
        join(&status);
        zap(termPID[i][2]);
        join(&status);
    }

    // eventually, at the end:
    quit(0);
    
}

int ClockDriver(char *arg)
{
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(semRunning);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while(! isZapped()) {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }
        /*
         * Compute the current time and wake up any processes
         * whose time has come.
         */
    }
    return -1;
}

void sleep(USLOSS_Sysargs* sysArgs){
    
}

int sleepReal(int secs){
    return -1;
}

int DiskDriver(char *arg){
    return -1;
}

void diskRead(USLOSS_Sysargs* sysArgs){

}

int diskReadReal(int unit, int track, int first, int sectors, void *buffer){
    return -1;
}

void diskWrite(USLOSS_Sysargs* sysArgs){

}

int diskWriteReal(int unit, int track, int first, int sectors, void *buffer){
    return -1;
}

void diskSize(USLOSS_Sysargs* sysArgs){

}

int diskSizeReal(int unit, int* sector, int* track, int* disk){
    return -1;
}

int TermDriver(char *arg){
    return -1;
}

int TermReader(char *arg){
    return -1;
}

void termRead(USLOSS_Sysargs* sysArgs){

}

int termReadReal(int unit, int size, char *buffer){
    return -1;
}

int TermWriter(char *arg){
    return -1;
}

void termWrite(USLOSS_Sysargs* sysArgs){

}

int termWriteReal(int unit, int size, char *text){
    return -1;
}

void initProc(int i){
    proc4Ptr current = &ProcTable[i];
    
    current->pid = -1;
    //TODO init mutex
    current->nextProcPtr = NULL;
    current->childPtr = NULL;
    current->nextSiblingPtr = NULL;
    current->parentPtr = NULL;
}