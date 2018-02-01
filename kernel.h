/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         parentProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr         quitChildPtr;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int             quitStatus;
   
   /*TP fields added */
   Queue		   quittedChildrenQueue; // Queue of the quitted children 	
   Queue		   zapQueue; // Queue of zapped processes 
   procPtr		   nextZapPtr; // From current process we can access next zap
   procPtr 		   nextQuittedSibPtr; // From current process we can access next quitted children 
   int			   zapped; // 1 if the process is zapped. 0 if it is not.

};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

/*TP*/
/* 
Struct for ZAP and QUITTEDCHILDREN queue.   
The define values are use in "union methods" which are 
used to identify which queue is working on 
*/
typedef struct Queue Queue;
#define QUITTEDCHILDREN 0
#define ZAP 1

/*TP*/
struct Queue {
	procPtr head;
	procPtr tail;
	int 	type; // ZAP or QUITTEDCHILDREN 
	int 	size; // size of the 
};


/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

/* process statuses */
#define EMPTYSTATUS 0
#define READYSTATUS 1
#define RUNSTATUS 2
#define QUITSTATUS 3
#define JOINBLOCKSTATUS 4

