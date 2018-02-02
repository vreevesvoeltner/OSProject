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
   int             startTime;
   
   /*TP fields added */	
   procPtr         zapPtr; // From current process we can access next zap
   procPtr         zappedByPtr;
   procPtr         nextZappedBy;

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
#define ZAPSTATUS 5

