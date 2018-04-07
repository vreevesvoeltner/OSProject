/* ------------------------------------------------------------------------
phase4.c .
Students:
Veronica Reeves
Thai Pham

University of Arizona
Computer Science 452

Summary:
Continue the implementation of the support level that was begun in phase
3. All of the services in phase 4 will be requested by user programs 
through the system call interface. For this phase, the support level will
contain system driver processes to handle the pseudo-clock, terminal input,
and disk I/O. These system processes run in kernel mode and with interrupts
enabled. Similar to phase3, when an error occurs the operating system should 
not crash when an error is made by a user-level process. Instead, the 
offending process is terminated (not quit).
------------------------------------------------------------------------ */

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

int  semRunning,
     diskPID[USLOSS_DISK_UNITS],
     termPID[USLOSS_TERM_UNITS][3];

proc4 ProcTable[MAXPROC];

proc4Ptr sleepLst = NULL,
         diskLst[USLOSS_DISK_UNITS];

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
int getTime();
void initProc(int);
void setUserMode();

//int diskReadOrWriteReal(int, int, int, int, void *, int);

void start3(void){
    char    name[128],
            buf[10],
            filename[50];
    int     i,
            pid,
            clockPID,
            status,
            tempSec,
            tempTrack;
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
        sprintf(buf, "%d", i);
        diskPID[i] = fork1("Disk Driver", DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (diskPID[i] < 0) {
            USLOSS_Console("start3(): Can't create disk driver\n");
            USLOSS_Halt(1);
        }
        ProcTable[diskPID[i] % MAXPROC].pid = diskPID[i];
        diskLst[i] = NULL;
        sempReal(semRunning);
        
        diskSizeReal(i, &tempSec, &tempTrack, &ProcTable[diskPID[i] % MAXPROC].track);
    }

    // May be other stuff to do here before going on to terminal drivers
/*
     for (i = 0; i < USLOSS_TERM_UNITS; i++){
        //Initialize term mailboxes
        sprintf(buf, "%d", i); 
        termPID[i][0] = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
        if (termPID[i][0] < 0) {
            USLOSS_Console("start3(): Can't create term driver\n");
            USLOSS_Halt(1);
        }
        ProcTable[termPID[i][0] % MAXPROC].pid = termPID[i][0];
        sempReal(semRunning);
        
        termPID[i][1] = fork1(name, TermReader, buf, USLOSS_MIN_STACK, 2);
        if (termPID[i][1] < 0) {
            USLOSS_Console("start3(): Can't create term reader\n");
            USLOSS_Halt(1);
        }
        ProcTable[termPID[i][1] % MAXPROC].pid = termPID[i][1];
        sempReal(semRunning);
        
        termPID[i][2] = fork1(name, TermWriter, buf, USLOSS_MIN_STACK, 2);
        if (termPID[i][2] < 0) {
            USLOSS_Console("start3(): Can't create term writer\n");
            USLOSS_Halt(1);
        }
        ProcTable[termPID[i][2] % MAXPROC].pid = termPID[i][2];
        sempReal(semRunning);
     }
*/
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
        semvReal(ProcTable[diskPID[i]].waitSem);
        zap(diskPID[i]);
        join(&status);
    }
/*   
    for (i = 0; i < USLOSS_TERM_UNITS; i++){
        FILE *f;
        
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
*/
    // eventually, at the end:
    quit(0);
    
}

int ClockDriver(char *arg){
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
        proc4Ptr temp;
        while (sleepLst != NULL && sleepLst->sleepTime <= getTime()){
            temp = sleepLst;
            sleepLst = temp->nextSleepPtr;
            semvReal(temp->waitSem);
        }
    }
    return 0;
}
/*
Delays the calling process for the specified number of seconds (sleep).
Input
arg1: number of seconds to delay the process.
Output
arg4: -1 if illegal values are given as input; 0 otherwise.
*/
void sleep(USLOSS_Sysargs* sysArgs){
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("sleep(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    sysArgs->arg4 = (void*)(long)sleepReal((int)(long)sysArgs->arg1);
    setUserMode();
}
/*
Causes the calling process to become unrunnable for at least the specified number of
seconds, and not significantly longer. The seconds must be non-negative.
Return values:
-1: seconds is not valid
0: otherwise
*/
int sleepReal(int secs){
    proc4Ptr current = &ProcTable[getpid() % MAXPROC];
    
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("sleepReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    if (secs < 0)
        return -1;
    
    if (current->pid != getpid()){
        initProc(getpid());
        current->pid = getpid();
    }
    
    current->sleepTime = getTime() + secs * 1000000;
    
    //Add current proc to list of sleeping processes
    if (sleepLst == NULL){
        sleepLst = current;
    }else{
        if (current->sleepTime <= sleepLst->sleepTime){
            current->nextSleepPtr = sleepLst;
            sleepLst = current;
        }else{
            proc4Ptr temp = sleepLst;
            while (temp->nextSleepPtr != NULL && temp->nextSleepPtr->sleepTime < current->sleepTime){
                temp = temp->nextSleepPtr;
            }
            current->nextSleepPtr = temp->nextSleepPtr;
            temp->nextSleepPtr = current;
        }
    }
    
    sempReal(current->waitSem); // Block until awakened
    
    return 0;
}

int DiskDriver(char *arg){
    int unit = atoi((char*)arg),
        result,
        status;
    
    semvReal(semRunning);
    while (!isZapped()){
        sempReal(ProcTable[getpid() % MAXPROC].waitSem);
        if (isZapped())
            break;
            
        if (diskLst[unit] != NULL){
            proc4Ptr current = diskLst[unit];
            diskLst[unit] = current->nextDiskPtr;
            current->nextDiskPtr = NULL;
            
            if (current->request.opr == USLOSS_DISK_TRACKS){
                USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &current->request);
                result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                
                if (result != 0) {
                    return 0;
                }
            }else{
                //do reading and writing
            }
            
            semvReal(current->waitSem);
        }
        continue;
    }
    return 0;
}

void diskRead(USLOSS_Sysargs* sysArgs){
    int unit = (int)(long)sysArgs->arg5,
        track = (int)(long)sysArgs->arg3,
        first = (int)(long)sysArgs->arg4,
        sectors = (int)(long)sysArgs->arg2,
        result;
        
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("diskRead(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
        
    result = diskReadReal(unit, track, first, sectors, sysArgs->arg1);

    sysArgs->arg1 = (void*)(long)result;
    sysArgs->arg4 = (void*)((long)(result != -1) - 1);
    
    setUserMode();
}

/*
Reads sectors sectors from the disk indicated by unit, starting at track track and
sector first. The sectors are copied into buffer. Your driver must handle a range of
sectors specified by first and sectors that spans a track boundary (after reading the
last sector in a track it should read the first sector in the next track). A file cannot wrap
around the end of the disk.
Return values:
-1: invalid parameters
0: sectors were read successfully
>0: disk�s status register
*/
int diskReadReal(int unit, int track, int first, int sectors, void *buffer){
    return -1;
}
/*
Writes one or more sectors to the disk (diskWrite).
Input
arg1: the memory address from which to transfer.
arg2: number of sectors to write.
arg3: the starting disk track number.
arg4: the starting disk sector number.
arg5: the unit number of the disk to write.
Output
arg1: 0 if transfer was successful; the disk status register otherwise.
arg4: -1 if illegal values are given as input; 0 otherwise.
The arg4 result is only set to -1 if any of the input parameters are obviously invalid, e.g. the
starting sector is negative.
*/
void diskWrite(USLOSS_Sysargs* sysArgs){
    int unit = (int)(long)sysArgs->arg5,
        track = (int)(long)sysArgs->arg3,
        first = (int)(long)sysArgs->arg4,
        sectors = (int)(long)sysArgs->arg2,
        result;
        
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("diskWrite(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
        
    result = diskWriteReal(unit, track, first, sectors, sysArgs->arg1);
    
    sysArgs->arg1 = (void*)(long)result;
    sysArgs->arg4 = (void*)((long)(result != -1) - 1);
    
    setUserMode();
}

/*
Writes sectors sectors to the disk indicated by unit, starting at track track and sector
first. The contents of the sectors are read from buffer. Like diskRead, your driver
must handle a range of sectors specified by first and sectors that spans a track
boundary. A file cannot wrap around the end of the disk.
Return values:
-1: invalid parameters
0: sectors were written successfully
>0: disk�s status register
*/
int diskWriteReal(int unit, int track, int first, int sectors, void *buffer){
    
    return -1;
}
/*
Returns information about the size of the disk (diskSize).
Input
arg1: the unit number of the disk
Output
arg1: size of a sector, in bytes
arg2: number of sectors in a track
arg3: number of tracks in the disk
arg4: -1 if illegal values are given as input; 0 otherwise.
*/
void diskSize(USLOSS_Sysargs* args) {
    int unit = (long) args->arg1,
        sector,
        track,
        disk;
        
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("diskSize(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    args->arg4 = (void*)(long)diskSizeReal(unit, &sector, &track, &disk);
    args->arg1 = (void*)(long)sector;
    args->arg2 = (void*)(long)track;
    args->arg3 = (void*)(long)disk;
    
    setUserMode();
}
/*
Returns information about the size of the disk indicated by unit. The sector parameter
is filled in with the number of bytes in a sector, track with the number of sectors in a
track, and disk with the number of tracks in the disk.
Return values:
-1: invalid parameters
0: disk size parameters returned successfully
*/
int diskSizeReal(int unit, int* sector, int* track, int* disk){
    proc4Ptr driver,
             current;
    USLOSS_DeviceRequest r;

    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("diskSizeReal(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    if (unit < 0 || unit > USLOSS_DISK_UNITS || sector == NULL || track == NULL || disk == NULL) {
        if (DEBUG4)
            USLOSS_Console("diskSizeReal: given illegal argument(s)\n");
        return -1;
    }
    
    driver = &ProcTable[diskPID[unit]];
    
    if (driver->track == -1){
        current = &ProcTable[getpid() % MAXPROC];
        if (current->pid != getpid()){
            initProc(getpid() % MAXPROC);
            current->pid = getpid();
        }
        
        current->track = 0;
        r.opr = USLOSS_DISK_TRACKS;
        r.reg1 = &driver->track;
        current->request = r;
        
        if (diskLst[unit] == NULL){
            diskLst[unit] = current;
        }else{
            if (diskLst[unit]->track > current->track){
                current->nextDiskPtr = diskLst[unit];
                diskLst[unit] = current;
            }else{
                proc4Ptr temp = diskLst[unit];
                while (temp->nextDiskPtr != NULL && temp->nextDiskPtr->track <= current->track){
                    temp = temp->nextDiskPtr;
                }
                current->nextDiskPtr = temp->nextDiskPtr;
                temp->nextDiskPtr = current;
            }
        }
        
        semvReal(driver->waitSem);
        sempReal(current->waitSem);
    }
    
    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;
    *disk = driver->track;
    
    return 0;
}

int TermDriver(char *arg){
    int unit = atoi((char*)arg),
        status,
        result;
    
    semvReal(semRunning);
    while (!isZapped()){
        result = waitDevice(USLOSS_TERM_INT, unit, &status);
        if (result != 0) /*process was zapped while waiting*/
            return 0;
            
        /*use MboxCondSend with chaWrite and charRead mailboxes to try
          writing and reading. USLOSS_TERM_STAT_RECV gives status
          for receiving and USLOSS_TERM_STAT_XMIT gives status
          for sending. Use the unit to determine which mailboxes to use.*/
    }
    return 0;
}

int TermReader(char *arg){
    semvReal(semRunning);
    while (!isZapped()){
        //Receive from charRead mailbox corresponding to this unit
        //When a full line is read send it to the lineSend mailbox corresponding to this unit
    }
    return -1;
}

/*
Read a line from a terminal (termRead).
Input
arg1: address of the user�s line buffer.
arg2: maximum size of the buffer.
arg3: the unit number of the terminal from which to read.
Output
arg2: number of characters read.
arg4: -1 if illegal values are given as input; 0 otherwise.
*/
void termRead(USLOSS_Sysargs* sysArgs){
    int unit = (int)(long)sysArgs->arg3,
        size = (int)(long)sysArgs->arg2,
        result;
        
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("termRead(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
        
    result = termReadReal(unit, size, sysArgs->arg1);
    
    sysArgs->arg2 = (void*)(long)result;
    sysArgs->arg4 = (void*)((long)(result != -1) - 1);
    
    setUserMode();
}

int termReadReal(int unit, int size, char *buffer){
    return -1;
}
/*
Write a line to a terminal (termWrite).
Input
arg1: address of the user�s line buffer.
arg2: number of characters to write.
arg3: the unit number of the terminal to which to write.
Output
arg2: number of characters written.
arg4: -1 if illegal values are given as input; 0 otherwise.
*/
int TermWriter(char *arg){
    semvReal(semRunning);
    while (!isZapped()){
        continue;
    }
    return -1;
}

void termWrite(USLOSS_Sysargs* sysArgs){
    int unit = (int)(long)sysArgs->arg3,
        size = (int)(long)sysArgs->arg2,
        result;
        
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("termWrite(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
        
    result = termWriteReal(unit, size, sysArgs->arg1);
    
    sysArgs->arg2 = (void*)(long)result;
    sysArgs->arg4 = (void*)((long)(result != -1) - 1);
    
    setUserMode();
}
/*This routine writes size characters � a line of text pointed to by text to the terminal
indicated by unit. A newline is not automatically appended, so if one is needed it must
be included in the text to be written. This routine should not return until the text has been
written to the terminal.
Return values:
-1: invalid parameters
>0: number of characters written
*/
int termWriteReal(int unit, int size, char *text){
    return -1;
}

void initProc(int i){
    proc4Ptr current = &ProcTable[i];
    
    current->pid = -1;
    current->track = -1;
    current->waitSem = semcreateReal(0);
    current->sleepTime = -1;
    current->nextProcPtr = NULL;
    current->nextDiskPtr = NULL;
    current->nextSleepPtr = NULL;
}

int getTime(){
    int currentTime;
    USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &currentTime);
    return currentTime;
}

/*    
setUserMode()
    Switch from kernel to user mode 
*/
void setUserMode(){
    int r = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
    if (DEBUG4) {
        USLOSS_Console("%d get way from warning unused variable\n", r);
    }
}