
#define DEBUG2 1

typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;

struct mailbox {
    int       mboxID;
    int       numMessages; //Number of messages (slots in use) in this mailbox
    int       totalSlots; //Max number of slots in this box
    int       slotSize; //Max size of the slots
    slotPtr   slots; //pointer to the first slot in this mailbox
    // other items as needed...
};

struct mailSlot {
    int       mboxID;
    int       status;
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

#define SLOTEMPTY 0
#define SLOTFULL 1
