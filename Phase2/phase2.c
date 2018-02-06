/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <stdlib.h>

#include <message.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

int nextMboxID;

int numMailBoxes = 0;

mailSlot MailSlotTable[MAXSLOTS];

slotPtr nextOpenSlot;

int filledSlots = 0;

procStruc ProcTable[MAXPROC];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...




/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kidPid;
    int status;
    int i;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    // Initialize the mail box table, slots, & other data structures.
    for (i = 0; i < MAXMBOX; i++){
        initMailBox(MailBoxTable[i]);
    }
    nextMboxID = 0;
    
    for (i = 0; i < MAXSLOTS; i++){
        initMailSlot(MailSlotTable[i]);
    }
    nextOpenSlot = &MailSlotTable[0];
    // Initialize USLOSS_IntVec and system call handlers,
    
    // allocate mailboxes for interrupt handlers.  Etc... 

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kidPid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kidPid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    // check if there are any open mailboxes
    if (numMailBoxes == MAXMBOX){
        USLOSS_Console("MboxCreate(): no open mailboxes.);
        return -1;
    }
    // check if parameters are valid
    if (slots > MAXSLOTS || slots < 0 || slot_size > MAX_MESSAGE || slot_size < 0){
        USLOSS_Console("MboxCreate(): invalid parameters.");
        return -1;
    }
    
    mbox = MailBoxTable[nextMboxID % MAXMBOX];
    nextMboxID++;
    while (mbox.mboxID != -1){
        mbox = MailBoxTable[nextMboxID % MAXMBOX];
        nextMboxID++;
    }
    
    mbox.mboxID = nextMboxID - 1;
    mbox.totalSlots = slots;
    mbox.slotSize = slot_size;
    
    return mbox.mboxID;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    return 0;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    return 0;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - Determine if there any processes blocked on any of the
             interrupt mailboxes.
   Returns - 1 if one (or more) processes are blocked; 0 otherwise
   Side Effects - none.

   Note: Do nothing with this function until you have successfully completed
   work on the interrupt handlers and their associated mailboxes.
   ------------------------------------------------------------------------ */
int check_io(void)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("check_io(): called\n");
    return 0;
} /* check_io */

void initMailBox(mailbox m){
    m.mboxID = -1;
    m.numMessages = 0;
    m.totalSlots = 0;
    m.slotSize = 0;
    m.slots = NULL;
}

void initMailSlot(mailSlot s){
    s.mboxID = -1;
    s.status = SLOTEMPTY;
    s.nextSlot = NULL;
}
