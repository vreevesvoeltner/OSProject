
#define DEBUG2 1

typedef struct mailSlot mailSlot;
typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc mboxProc;
typedef struct mboxProc *mboxProcPtr;


struct mailbox {
    int       mboxID;
    int       numMessages; //Number of messages (slots in use) in this mailbox
    int       totalSlots; //Max number of slots in this box
    int       slotSize; //Max size of the slots
    slotPtr   slots; //pointer to the first slot in this mailbox
    // other items as needed...
	//-----TP----------
	mboxProcPtr blockedReceivePrt; // pointing to the first pointer in the list
	//-----------------
};

struct mailSlot {
    int       mboxID;
    int       status;
    int       msgSize; 
    char      message[MAX_MESSAGE];
    slotPtr   nextSlot; //Next mailbox slot in the same mailbox
    // other items as needed...
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

//-------------TP-------------//
/* ------------------------------------------------------------------------
Store information of the process and message that are blocked by "send" or 
"receive" 
------------------------------------------------------------------------ */
struct mboxProc {
	int          processPID;// current process ID that calls "receive" or "send" 
	int          msgSize;
	void        *msgPtr; // received message
	mboxProcPtr  nextMboxProc;
};


#define SLOTEMPTY 0
#define SLOTFULL 1
#define EMPTY_BOX 15 // There is no message in the current mail box