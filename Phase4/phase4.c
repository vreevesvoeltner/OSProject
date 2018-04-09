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
#include <stdio.h>
#include <string.h>

int warningGAW = 0;
int  semRunning,
     diskPID[USLOSS_DISK_UNITS],
     termPID[USLOSS_TERM_UNITS][3];

proc4 ProcTable[MAXPROC];

proc4Ptr sleepLst = NULL;

/*    
    we use circular algorithm to decide which 
    is the next process will read/write. It means 
    that we need to move forward from the "current"
    we are at. There are 2 case: 
        - The new added process requests read track 
          that => current reading track. 
        - The new added process requests read the track 
          that < the current reading track
    So, we need to keep track of current track and the 
    process has smallest track in the diskLst
*/
//proc4Ptr diskLst[USLOSS_DISK_UNITS];
diskList diskLst [USLOSS_DISK_UNITS];
proc4Ptr dCurrs[USLOSS_DISK_UNITS]; // Current read/write positions 

// Terminal mailboxes and controls
int  charRead[USLOSS_TERM_UNITS],
     charWrite[USLOSS_TERM_UNITS],
     lineRead[USLOSS_TERM_UNITS],
     lineWrite[USLOSS_TERM_UNITS],
     writeProc[USLOSS_TERM_UNITS],
     recvIntEnabled[USLOSS_TERM_UNITS],
     xmitIntEnabled[USLOSS_TERM_UNITS];

//==============
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
proc4Ptr removeDisk(diskList*,int);
void termRead(USLOSS_Sysargs*);
int termReadReal(int, int, char *);
void termWrite(USLOSS_Sysargs*);
int termWriteReal(int, int, char *);
int getTime();
void initProc(int);
void setUserMode();

//int diskReadOrWriteReal(int, int, int, int, void *, int);

void start3(void)
{
    char    name[128],
            buf[10],
            filename[50];
    int     i = -1,
            pid= -1,
            clockPID = -1,
            status= - 1,
            tempSec = -1,
            tempTrack= -1;
    /*
     * Check kernel mode here.
     */
    if (warningGAW==1) {
        USLOSS_Console("%d,%d%d%d%d%d%s%s%s get way from warning unused variable\n", i,pid,clockPID,status,tempSec,tempTrack,name[128],buf[10],filename[50]);
    }
     
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
        sprintf(buf, "%d", i);
        pid = fork1("Disk Driver", DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (diskPID[i] < 0) {
            USLOSS_Console("start3(): Can't create disk driver\n");
            USLOSS_Halt(1);
        }
        
        //USLOSS_Console("======>pid:%d\n",pid);
        diskPID[i] = pid;
        //ProcTable[diskPID[i] % MAXPROC].pid = diskPID[i];
        //diskLst[i] = NULL;
        sempReal(semRunning);
        
        diskSizeReal(i, &tempSec, &tempTrack, &ProcTable[diskPID[i] % MAXPROC].track);
    }

    // May be other stuff to do here before going on to terminal drivers

     for (i = 0; i < USLOSS_TERM_UNITS; i++){
        //Initialize term mailboxes
        charRead[i] = MboxCreate(1, MAXLINE);
        charWrite[i] = MboxCreate(1, MAXLINE);
        lineRead[i] = MboxCreate(10, MAXLINE);
        lineWrite[i] = MboxCreate(10, MAXLINE);
        writeProc[i] = MboxCreate(1, sizeof(int));
        recvIntEnabled[i] = 0;
        xmitIntEnabled[i] = 0;

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
        semvReal(ProcTable[diskPID[i]].waitSem);
        zap(diskPID[i]);
        join(&status);
    }
  
    for (i = 0; i < USLOSS_TERM_UNITS; i++){
        FILE *f;
        int result;
        
        // Zap TermReader
        MboxSend(charRead[i], NULL, 0);
        zap(termPID[i][1]);
        join(&status);
        
        // Zap TermWriter
        MboxSend(lineWrite[i], NULL, 0);
        zap(termPID[i][2]);
        join(&status);
        
        // Zap TermDriver
        result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)(long)USLOSS_TERM_CTRL_RECV_INT(0));
        
        if (warningGAW==1) {
        USLOSS_Console("%d,get way from warning unused variable\n",result);
        }
        sprintf(filename, "term%d.in", i);
        f = fopen(filename, "a+");
        fprintf(f, "last line\n");
        fflush(f);
        fclose(f);
        zap(termPID[i][0]);
        join(&status);
    }

    // eventually, at the end:
    quit(0);
    
}

int ClockDriver(char *arg){
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(semRunning);
    int r = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (warningGAW==1) {
        USLOSS_Console("%d get way from warning unused variable\n", r);
    }
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
    proc4Ptr me = &ProcTable[getpid() % MAXPROC];
    initProc(getpid()%MAXPROC);
    diskLst[unit].head = NULL;
    diskLst[unit].tail = NULL;
    diskLst[unit].size = 0;
    
    semvReal(semRunning);
    int r = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (warningGAW==1) {
        USLOSS_Console("%d get way from warning unused variable\n", r);
    }
    while (!isZapped()){
        sempReal(me->waitSem);
        if (isZapped())
            return 0;
        
        if (diskLst[unit].size > 0){
            proc4Ptr temp;
            // peek current track  
            if (dCurrs[unit] == NULL){ // first time seeking 
                dCurrs[unit] = diskLst[unit].head;
            }
            temp = dCurrs[unit];
            int track = dCurrs[unit]->track;
            
            // Case1: Tracks request
            if (dCurrs[unit]->request.opr == USLOSS_DISK_TRACKS){
                r =  USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &dCurrs[unit]->request);
                if (warningGAW==1) {
                    USLOSS_Console("%d get way from warning unused variable\n", r);
                }
                result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                if (result != 0) {
                    return 0;
                }
            }else{//Case2: reading and writing
                while(dCurrs[unit]->sectors > 0){
                    USLOSS_DeviceRequest request;
                    request.opr = USLOSS_DISK_SEEK;
                    request.reg1 = track;
                    r = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
                    if (warningGAW==1) {
                        USLOSS_Console("%d get way from warning unused variable\n", r);
                    }
                    // Wait for the interrupt to occur
                    result = waitDevice(USLOSS_DISK_DEV, unit, &status);  
                    if (result != 0) {
                        return 0;
                    }
                    int fSec; 
                    for (fSec= dCurrs[unit]->firstSector; dCurrs[unit]->sectors > 0 && fSec < USLOSS_DISK_TRACK_SIZE; fSec++) {
                        dCurrs[unit]->request.reg1 = (void *) ((long) fSec);
                        r  = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &dCurrs[unit]->request);
                        if (warningGAW==1) {
                            USLOSS_Console("%d get way from warning unused variable\n", r);
                        }
                        result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                        if (result != 0) {
                            return 0;
                        }
                        dCurrs[unit]->sectors--;
                        dCurrs[unit]->request.reg2 += USLOSS_DISK_SECTOR_SIZE;
                    }
                    track++;
                    dCurrs[unit]->firstSector = 0;
                }
            }
            // remove executed process from diskList
            removeDisk(&diskLst[unit],unit);
            semvReal(temp->waitSem);
        }
    }
    semvReal(semRunning);
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
>0: disk’s status register
*/
int diskReadReal(int unit, int track, int first, int sectors, void *buffer){
     proc4Ptr driver,
             current;
   // check for illegal args
    if (unit < 0 || unit > 1 || track < 0 || track > ProcTable[diskPID[unit]].track ||
        first < 0 || first > USLOSS_DISK_TRACK_SIZE || buffer == NULL  ||
        (first + sectors)/USLOSS_DISK_TRACK_SIZE + track > ProcTable[diskPID[unit]].track) {
        return -1;
    }
    
    driver = &ProcTable[diskPID[unit]];
    
    current = &ProcTable[getpid() % MAXPROC];
    if (current->pid != getpid()){
       initProc(getpid() % MAXPROC);
        current->pid = getpid();
    }
    
    current->request.opr = USLOSS_DISK_READ;
    current->request.reg2 = buffer;
    current->sectors = sectors;
    current->track = track;
    current->firstSector = first;
    
    // add to diskLst request 
    if (diskLst[unit].head == NULL) { 
        diskLst[unit].head = diskLst[unit].tail = current;
        diskLst[unit].head->nextDiskPtr = diskLst[unit].tail->nextDiskPtr = NULL;
        diskLst[unit].head->prevDiskPtr = diskLst[unit].tail->prevDiskPtr = NULL;
    }
    else {
        proc4Ptr prev = diskLst[unit].tail;
        proc4Ptr next = diskLst[unit].head;
        while (next != NULL && next->track <= current->track) {
            prev = next;
            next = next->nextDiskPtr;
            if (next == diskLst[unit].head)
                break;
        }
        prev->nextDiskPtr = current;
        current->prevDiskPtr = prev;
        if (next == NULL)
            next = diskLst[unit].head;
        current->nextDiskPtr = next;
        next->prevDiskPtr = current;
        if (current->track < diskLst[unit].head->track)
            diskLst[unit].head = current; 
        if (current->track>=diskLst[unit].tail->track)
            diskLst[unit].tail = current; 
    }
    diskLst[unit].size++;
    
        
    //=======================
    semvReal(driver->waitSem);  // wake up disk driver
    sempReal(current->waitSem); // block current 
    int st;
    int result = USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &st);
    
    return result;
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
>0: disk’s status register
*/
int diskWriteReal(int unit, int track, int first, int sectors, void *buffer){
        proc4Ptr driver,
             current;
   // check for illegal args
    if (unit < 0 || unit > 1 || track < 0 || track > ProcTable[diskPID[unit]].track ||
        first < 0 || first > USLOSS_DISK_TRACK_SIZE || buffer == NULL  ||
        (first + sectors)/USLOSS_DISK_TRACK_SIZE + track > ProcTable[diskPID[unit]].track) {
        return -1;
    }
    
    driver = &ProcTable[diskPID[unit]];
    
    current = &ProcTable[getpid() % MAXPROC];
    if (current->pid != getpid()){
       initProc(getpid() % MAXPROC);
        current->pid = getpid();
    }
    
    current->request.opr = USLOSS_DISK_WRITE;
    current->request.reg2 = buffer;
    current->sectors = sectors;
    current->track = track;
    current->firstSector = first;
    
    // add to diskLst request 
    if (diskLst[unit].head == NULL) { 
        diskLst[unit].head = diskLst[unit].tail = current;
        diskLst[unit].head->nextDiskPtr = diskLst[unit].tail->nextDiskPtr = NULL;
        diskLst[unit].head->prevDiskPtr = diskLst[unit].tail->prevDiskPtr = NULL;
    }
    else {
        proc4Ptr prev = diskLst[unit].tail;
        proc4Ptr next = diskLst[unit].head;
        while (next != NULL && next->track <= current->track) {
            prev = next;
            next = next->nextDiskPtr;
            if (next == diskLst[unit].head)
                break;
        }
        prev->nextDiskPtr = current;
        current->prevDiskPtr = prev;
        if (next == NULL)
            next = diskLst[unit].head;
        current->nextDiskPtr = next;
        next->prevDiskPtr = current;
        if (current->track < diskLst[unit].head->track)
            diskLst[unit].head = current; 
        if (current->track>=diskLst[unit].tail->track)
            diskLst[unit].tail = current; 
    }
    diskLst[unit].size++;
    
        
    //=======================
    semvReal(driver->waitSem);  // wake up disk driver
    sempReal(current->waitSem); // block current 
    int st;
    int result = USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &st);
    
    return result;
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
        
        /*
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
        */
        
        // ==================================
        if (diskLst[unit].head == NULL) { 
            diskLst[unit].head = diskLst[unit].tail = current;
            diskLst[unit].head->nextDiskPtr = diskLst[unit].tail->nextDiskPtr = NULL;
            diskLst[unit].head->prevDiskPtr = diskLst[unit].tail->prevDiskPtr = NULL;
        }
        else {
            proc4Ptr prev = diskLst[unit].tail;
            proc4Ptr next = diskLst[unit].head;
            while (next != NULL && next->track <= current->track) {
                prev = next;
                next = next->nextDiskPtr;
                if (next == diskLst[unit].head)
                    break;
            }
            prev->nextDiskPtr = current;
            current->prevDiskPtr = prev;
            if (next == NULL)
                next = diskLst[unit].head;
            current->nextDiskPtr = next;
            next->prevDiskPtr = current;
            if (current->track < diskLst[unit].head->track)
                diskLst[unit].head = current; 
            if (current->track>=diskLst[unit].tail->track)
                diskLst[unit].tail = current; 
        }
        diskLst[unit].size++;
        //====================================
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
        devStatus,
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
        devStatus = USLOSS_TERM_STAT_RECV(status); // get terminal device status
        if (devStatus == USLOSS_DEV_BUSY){
            MboxCondSend(charRead[unit], &status, sizeof(int));
        }else if (devStatus == USLOSS_DEV_ERROR){
            if (DEBUG4) 
                USLOSS_Console("TermDriver: receive error\n");
        }
        
        
        devStatus = USLOSS_TERM_STAT_XMIT(status);
        if (devStatus == USLOSS_DEV_READY){
            MboxCondSend(charWrite[unit], &status, sizeof(int));
        }else if(devStatus == USLOSS_DEV_ERROR){
            if (DEBUG4)
                USLOSS_Console("TermDiver: send error\n");
        }
    }
    return 0;
}

int TermReader(char *arg){
    int unit = atoi((char*)arg),
        intIn,
        i = 0;
    char line[MAXLINE];
    
    semvReal(semRunning);
    while (!isZapped()){
        MboxReceive(charRead[unit], &intIn, sizeof(int));
        line[i] = USLOSS_TERM_STAT_CHAR(intIn);
        i++;
        if (line[i-1] == '\n' || i == MAXLINE){
            line[i] = '\0';
            
            MboxSend(lineRead[unit], line, i);
            i = 0;
        }
    }
    return 0;

}

/*
Read a line from a terminal (termRead).
Input
arg1: address of the user’s line buffer.
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
        
    result = termReadReal(unit, size, sysArgs->arg1);
    
    sysArgs->arg2 = (void*)(long)result;
    sysArgs->arg4 = (void*)((long)(result != -1) - 1);
}

int termReadReal(int unit, int size, char *buffer){
    int lineSize,
        ctrl = 0,
        result;
    char line[MAXLINE];
    
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("termRead(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    if (unit < 0 || unit > USLOSS_TERM_UNITS - 1 || size <= 0)
        return -1;
    if (!recvIntEnabled[unit]){
        result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)USLOSS_TERM_CTRL_RECV_INT(ctrl));
        if (warningGAW==1) {
            USLOSS_Console("%d,get way from warning unused variable\n",result);
        }
        
    }
    recvIntEnabled[unit]++;
    
    lineSize = MboxReceive(lineRead[unit], &line, MAXLINE);
    if (lineSize > size)
        lineSize = size;
    memcpy(buffer, line, lineSize);
    
    recvIntEnabled[unit]--;
    ctrl = 0;
    if (!recvIntEnabled[unit] && !xmitIntEnabled[unit])
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)USLOSS_TERM_CTRL_RECV_INT(ctrl));
        
    return lineSize;
}
/*
Write a line to a terminal (termWrite).
Input
arg1: address of the user’s line buffer.
arg2: number of characters to write.
arg3: the unit number of the terminal to which to write.
Output
arg2: number of characters written.
arg4: -1 if illegal values are given as input; 0 otherwise.
*/
int TermWriter(char *arg){
    int unit = atoi((char*)arg),
        lineSize,
        devStatus,
        ctrl = 0,
        pid,
        result,
        i = 0;
    char line[MAXLINE];
    
    semvReal(semRunning);
    while (!isZapped()){
        lineSize = MboxReceive(lineWrite[unit], line, MAXLINE);
        
        if (isZapped())
            break;
        
        ctrl = 0;
        ctrl = USLOSS_TERM_CTRL_XMIT_INT(ctrl);
        ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);

        result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)ctrl);
        xmitIntEnabled[unit] = 1;
        if (warningGAW==1) {
            USLOSS_Console("%d,get way from warning unused variable\n",result);
        }
        
        for (i = 0; i < lineSize; i++){
            MboxReceive(charWrite[unit], &devStatus, sizeof(int));

            if (USLOSS_TERM_STAT_XMIT(devStatus) == USLOSS_DEV_READY) {
                ctrl = 0;
                ctrl = USLOSS_TERM_CTRL_CHAR(ctrl, line[i]); // set char to send
                ctrl = USLOSS_TERM_CTRL_XMIT_CHAR(ctrl);  // set send char
                ctrl = USLOSS_TERM_CTRL_XMIT_INT(ctrl);   // set xmit char
                ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);   // set receive char

               result =  USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)ctrl);
               if (warningGAW==1) {
                    USLOSS_Console("%d,get way from warning unused variable\n",result);
                }
            }
        }
        ctrl = 0;
        if (recvIntEnabled[unit])
            ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
        result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long) ctrl);
        xmitIntEnabled[unit] = 0; 
        
        if (warningGAW==1) {
            USLOSS_Console("%d,get way from warning unused variable\n",result);
        }
        MboxReceive(writeProc[unit], &pid, sizeof(int));
        semvReal(ProcTable[pid % MAXPROC].waitSem);
    }
    return 0;
}

void termWrite(USLOSS_Sysargs* sysArgs){
    int unit = (int)(long)sysArgs->arg3,
        size = (int)(long)sysArgs->arg2,
        result;
        
    result = termWriteReal(unit, size, sysArgs->arg1);
    
    sysArgs->arg2 = (void*)(long)result;
    sysArgs->arg4 = (void*)((long)(result != -1) - 1);
}
/*This routine writes size characters — a line of text pointed to by text to the terminal
indicated by unit. A newline is not automatically appended, so if one is needed it must
be included in the text to be written. This routine should not return until the text has been
written to the terminal.
Return values:
-1: invalid parameters
>0: number of characters written
*/
int termWriteReal(int unit, int size, char *text){
    int pid;
    
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("termWrite(): in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    if (unit < 0 || unit > USLOSS_TERM_UNITS - 1 || size < 0)
        return -1;
        
    pid = getpid();
    MboxSend(writeProc[unit], &pid, sizeof(int));
    
    MboxSend(lineWrite[unit], text, size);
    sempReal(ProcTable[pid % MAXPROC].waitSem);
    return size;
}

void initProc(int i){
    proc4Ptr current = &ProcTable[i];
    
    current->pid = -1;
    current->firstSector = -1;
    current->sectors = -1;
    current->track = -1;
    current->waitSem = semcreateReal(0);
    current->sleepTime = -1;
    current->nextProcPtr = NULL;
    current->nextDiskPtr = NULL;
    current->prevDiskPtr = NULL;
    current->nextSleepPtr = NULL;
}

int getTime(){
    int currentTime;
    int r = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &currentTime);
    if (warningGAW==1) {
        USLOSS_Console("%d get way from warning unused variable\n", r);
    }
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

proc4Ptr removeDisk(diskList* ld,int unit){
    if (ld->size == 0)
        return NULL;

    if (dCurrs[unit] == NULL) {
        dCurrs[unit] = ld->head;
    }

    proc4Ptr temp = dCurrs[unit];

    if (ld->size == 1) {
        ld->head = ld->tail = dCurrs[unit] = NULL;
    }

    else if (dCurrs[unit] == ld->head) {
        ld->head = ld->head->nextDiskPtr;
        ld->head->prevDiskPtr = ld->tail;
        ld->tail->nextDiskPtr = ld->head;
        dCurrs[unit] = ld->head;
    }

    else if (dCurrs[unit] == ld->tail) {
        ld->tail = ld->tail->prevDiskPtr;
        ld->tail->nextDiskPtr = ld->head;
        ld->head->prevDiskPtr = ld->tail;
        dCurrs[unit] = ld->head;
    }

    else {
        dCurrs[unit]->prevDiskPtr->nextDiskPtr = dCurrs[unit]->nextDiskPtr;
        dCurrs[unit]->nextDiskPtr->prevDiskPtr = dCurrs[unit]->prevDiskPtr;
        dCurrs[unit] = dCurrs[unit]->nextDiskPtr;
    }

    ld->size--;

    return temp;
}