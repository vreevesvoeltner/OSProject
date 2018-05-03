
#include <usyscall.h>
#include "usloss.h"
#include "vm.h"
#include "phase5.h"
#define DEBUG 0
extern int debugflag;
extern Process processes[MAXPROC];
extern FTEptr frameTable;
extern DTEptr diskTable;
extern void *vmRegion;
extern VmStats vmStats;

void p1_fork(int pid){
    if (vmRegion == NULL){
        if (DEBUG && debugflag)
            USLOSS_Console("p1_fork(): vmRegion not yet initialized.\n");
        return;
    }
    int i;
    ProcPtr proc = &processes[pid % MAXPROC];
    proc->pid = pid;
    
    proc->pageTable = malloc(proc->numPages * sizeof(PTE));
    
    for (i = 0; i < proc->numPages; i++){
        proc->pageTable[i].state = UNUSED;
        proc->pageTable[i].frame = -1;
        proc->pageTable[i].diskBlock = -1;
        proc->pageTable[i].nextPage = NULL;
    }
    
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void p1_switch(int old, int new){
    //USLOSS_Console("p1Switch(): old = %d, new = %d\n", old, new);
	int r;
    if (vmRegion == NULL){
        if (DEBUG && debugflag)
            USLOSS_Console("p1_switch(): vmRegion not yet initialized.\n");
        return;
    }
    int i;
    vmStats.switches++;
    
    if (old > 0){ //unload old process mappings
        int temp1, temp2,
            result;
        ProcPtr proc = &processes[old % MAXPROC];
        
        if (proc->pageTable != NULL){
            for (i = 0; i < proc->numPages; i++){
                result = USLOSS_MmuGetMap(TAG, i, &temp1, &temp2);
                if (result != USLOSS_MMU_ERR_NOMAP){
                    r = USLOSS_MmuUnmap(TAG, i);
                    if (DEBUG && debugflag)
                        USLOSS_Console("p1_switch(): unmapped page %d from proc %d\n remove waring%", i, old,r);
                }
            }
        }
    }
    
    if (new > 0){
        ProcPtr proc = &processes[new % MAXPROC];
        if (proc->pageTable != NULL){
            for (i = 0; i < proc->numPages; i++){
                if (proc->pageTable[i].state == INFRAME){
                    r = USLOSS_MmuMap(TAG, i, proc->pageTable[i].frame, USLOSS_MMU_PROT_RW);
                    
                    if (DEBUG && debugflag)
                        USLOSS_Console("p1_switch(): mapped page %d to frame %d for proc %d \n", i, proc->pageTable[i].frame, new);
                }
            }
        }
    }
    
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void p1_quit(int pid){
	int r;
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
        
    int i,
        result,
        frame,
        temp;
    
    if (vmRegion != NULL){
        ProcPtr proc = &processes[pid % MAXPROC];
        if (proc->pageTable != NULL){
            for (i = 0; i <proc->numPages; i++){
                result = USLOSS_MmuGetMap(TAG, i, &frame, &temp);
                if (result != USLOSS_MMU_ERR_NOMAP){
                    if (proc->pageTable[i].diskBlock != -1){
                        diskTable[proc->pageTable[i].diskBlock].pid = -1;
                        diskTable[proc->pageTable[i].diskBlock].page = -1;
                    }
                    r = USLOSS_MmuUnmap(TAG, i);
					if (1==0){
						USLOSS_Console("%d\n", r);
					}
                    
                    frameTable[frame].state = FUNUSED;
                    frameTable[frame].frame = -1;
                    frameTable[frame].page = -1;
                    frameTable[frame].pid = -1;
                    
                    vmStats.freeFrames++;
                }
                proc->pageTable[i].state = UNUSED;
                proc->pageTable[i].frame = -1;
                proc->pageTable[i].diskBlock = -1;
                proc->pageTable[i].nextPage = NULL;
            }
        }
        free(proc->pageTable);
    }
} /* p1_quit */
