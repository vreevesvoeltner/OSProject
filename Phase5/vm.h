/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
/* You'll probably want more states */

/*
 * Different states for a data block
 */
#define DUNUSED 600
#define DINUSE  601

/*
 * Different States for a frame
 */
#define FUNUSED 700
#define FINUSE  701

/*
 * Page table entry.
 */
typedef struct PTE PTE;
typedef struct PTE* PTEptr;
typedef struct FTE FTE;
typedef struct FTE* FTEptr;
typedef struct Process Process;
typedef struct Process* ProcPtr;
typedef struct FaultMsg FaultMsg;
 
struct PTE {
    int  state,      // See above.
         frame,      // Frame that stores the page (if any). -1 if none.
         diskBlock;  // Disk block that stores the page (if any). -1 if none.
    PTEptr nextPage;
};

/*
 * Frame table entry
 */
// Create a struct for your frame table:
struct FTE{
    int state,  // Frame state, values to be added above like with PTE
        frame,  // The number of the frame
        page,   // The page that uses this frame, -1 if none
        pid;    // The process number that owns the frame
    FTEptr nextFrame;
};

/*
 * Per-process information.
 */
struct Process {
    int  numPages,   // Size of the page table.
         faultMbox;  // Used to wait for fault resolution
    PTE  *pageTable; // The page table for the process.
};

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
};

