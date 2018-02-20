/* ------------------------------------------------------------------------
   phase2.c v2
   Students: 
   Veronica Reeves
   Thai Pham

   University of Arizona
   Computer Science 452
   
   Summary:
   For this second phase of the operating system, we will implement low-level 
   process synchronization and communication via messages, and interrupt 
   handlers. This phase, combined with phase 1, provides the building blocks
   needed by later phases, which will implement more complicated 
   process-control functions, device drivers, and virtual memory.
   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include "phase1.h"
#include "phase2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "phase1.h"
#include "message.h"


/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
void initMailBox (mailbox*);
void initMailSlot (slotPtr);
void disableInterrupts();
void enableInterrupts();


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int debugflagRelease = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

int nextMboxID;

int numMailBoxes = 0;

mailSlot MailSlotTable[MAXSLOTS];

int nextOpenSlot;

int filledSlots = 0;

//procStruc ProcTable[MAXPROC];

// also array of function ptrs to system call handlers, ...




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
	// Make sure in Kernel mode
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	disableInterrupts();

    int kidPid;
    int status;
    int i;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    // Initialize the mail box table, slots, & other data structures.
    for (i = 0; i < MAXMBOX; i++){
        initMailBox(&MailBoxTable[i]);
    }
    nextMboxID = 0;
    
    for (i = 0; i < MAXSLOTS; i++){
        initMailSlot(&MailSlotTable[i]);
    }
    nextOpenSlot = 0;
    // Initialize USLOSS_IntVec and system call handlers,
    
    // allocate mailboxes for interrupt handlers.  Etc... 
    for (i = 0; i < 7; i++){
        MboxCreate(0, 0);
    }

	enableInterrupts();
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
	// Make sure in Kernel mode
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	disableInterrupts();

    // check if there are any open mailboxes
    if (numMailBoxes == MAXMBOX){
        //USLOSS_Console("MboxCreate(): no open mailboxes.");
        return -1;
    }
    // check if parameters are valid
    if (slots > MAXSLOTS || slots < 0 || slot_size > MAX_MESSAGE || slot_size < 0){
        USLOSS_Console("MboxCreate(): Invalid arguments given.\n");
        return -1;
    }
    
    mailbox* mbox = &MailBoxTable[nextMboxID % MAXMBOX];
    nextMboxID++;
    while (mbox->mboxID != -1){
        mbox = &MailBoxTable[nextMboxID % MAXMBOX];
        nextMboxID++;
    }
    
    mbox->mboxID = nextMboxID - 1; // FIXME: need do a % operation 
    mbox->totalSlots = slots;
    mbox->slotSize = slot_size;
  
    numMailBoxes++;

	enableInterrupts();
    return mbox->mboxID;
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
	// Make sure in Kernel mode
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	disableInterrupts();

    //Check if there are available slots in the SlotTable[] 
    if (filledSlots == MAXSLOTS){
        USLOSS_Console("MboxSend(): Maximum slots reached. Halting...\n");
        USLOSS_Halt(1);
    }
    
	//Check if mailbox has been released
    if (MailBoxTable[mbox_id % MAXMBOX].mboxID == -1){
        USLOSS_Console("MboxSend(): Given mailbox has been released.\n");
        return -3;
    }
    
    mailbox *target = &MailBoxTable[mbox_id % MAXMBOX];
    slotPtr temp,
            newSlot;
    
    //Check if arguments are valid
    if (mbox_id < 0 || msg_size < 0 || msg_size > target->slotSize){
        USLOSS_Console("MboxSend(): Invalid argument given.\n");
        return -1;
    }
    

	/*TP
	If there is receiver is waiting for the message
		send message to the waiting receiver
		and unlocked the waiting receiver 
	If there is no receiver
		add new message to the mailbox slots 
		and wait for receiver
	*/
	if (target->blockedReceivePrt != NULL) {

		if (DEBUG2 && debugflag2) {
			USLOSS_Console("-> MboxSend():There is receiver waiting at boxID: %d\n", target->mboxID);
		}
		
		// get the waiting receiver
		mboxProcPtr waitingReceiver = target->blockedReceivePrt;

		// send message to waiting receiver 
		waitingReceiver->msgSize = msg_size;
		memcpy(waitingReceiver->msgPtr, msg_ptr, msg_size);

		// remove and reset pointer in waiting receiver
		target->blockedReceivePrt = waitingReceiver->nextMboxProc;
		waitingReceiver->nextMboxProc = NULL;

		//
		if (DEBUG2 && debugflag2) {
			USLOSS_Console("-> MboxSend():did send message to process PID: %d at boxID: %d\n",waitingReceiver->processPID, target->mboxID);
		}

		// unblocked the calling receiver`s process 
		unblockProc(waitingReceiver->processPID);
	}
	else { /// no waiting receiver 
		/*TP
		Block the sender if this mailbox has no available slots. 
		For example, mailbox has 5 slots and all of them already 
		have a message.
		After block sender, add new message to block sender list
		*/
		
		

		if (target->numMessages == target->totalSlots) {
			if (DEBUG2 && debugflag2) {
				USLOSS_Console("-> MboxSend():Slots at boxID: %d is FULL.\n", target->mboxID);
				USLOSS_Console("-> MboxSend(): %d in use. Max available slots %d.\n", target->numMessages, target->totalSlots);
			}
			//  Create new mail box  process 
			// variables needed .TP

			mboxProc aBoxProc; // new mail box process which is in blocked list 
			aBoxProc.processPID = getpid();
			aBoxProc.msgSize = msg_size;
			aBoxProc.msgPtr = msg_ptr;
			aBoxProc.nextMboxProc = NULL;

			// Adding the new mail box process to the end of block send list to wait for receiver 
			if (target->blockedSendPrt == NULL) {
				target->blockedSendPrt = &aBoxProc;
				if (DEBUG2 && debugflag2) {
					USLOSS_Console("->-> MboxSend():  First new mail process successful added to blockSend.\n Mail box ID: %d, Message size:%d \n", target->mboxID, aBoxProc.msgSize);
				}
			}
			else {
				mboxProcPtr temp = target->blockedSendPrt;
				while (temp->nextMboxProc != NULL) {
					temp = temp->nextMboxProc;
				}
				temp->nextMboxProc = &aBoxProc;
				if (DEBUG2 && debugflag2) {
					USLOSS_Console("->-> MboxSend():  New mail process successful added to blockSend.\n Mail box ID: %d, Message size:%d \n", target->mboxID, aBoxProc.msgSize);
				}
			}

			// block the process calling send
			blockMe(FULL_BOX);
			disableInterrupts();
			//Check if process was zapped
			if (isZapped()) {
				USLOSS_Console("MboxSend(): Process was zapped while sending.\n");
				enableInterrupts();
				return -3;
			}
			
		}
		else {//Add message to mailbox
			if (DEBUG2 && debugflag2) {
				USLOSS_Console("-> MboxSend():There is NO receiver waiting. Add new message to slots at boxID: %d.\n", target->mboxID);
				USLOSS_Console("-> MboxSend(): %d in use. Max available slots %d.\n", target->numMessages, target->totalSlots);
			}

			newSlot = &MailSlotTable[nextOpenSlot];
			newSlot->mboxID = mbox_id;
			newSlot->status = SLOTFULL;
						
			if (target->slots == NULL) {
				target->slots = newSlot;
				newSlot->nextSlot = NULL;
			}
			else {
				temp = target->slots;
				while (temp->nextSlot != NULL) {
					temp = temp->nextSlot;
				}
				temp->nextSlot = newSlot;
				newSlot->nextSlot = NULL;
			}

			memcpy(newSlot->message, msg_ptr, msg_size);
			newSlot->msgSize = msg_size;

			target->numMessages++;
			filledSlots++;

			if (filledSlots != MAXSLOTS) {
				while (MailSlotTable[nextOpenSlot].mboxID != -1) {
					nextOpenSlot = (nextOpenSlot + 1) % MAXSLOTS;
				}
			}
		}
		
	}

	enableInterrupts();
    return 0;
} /* MboxSend */

  /* ------------------------------------------------------------------------
  Name - MboxRelease
  Purpose - Releases a previously created mailbox. 
	Any process can release any mailbox.
	The code for MboxRelease will need to devise a means of handling processes that are
	blocked on a mailbox being released. Essentially, each blocked process should return -3
	from the send or receive that caused it to block. The process that called MboxRelease
	needs to unblock all the blocked processes. When each of these processes awake from the
	blockMe call inside send or receive, it will need to �notice� that the mailbox has been
	released�
  Parameters - ID of the mailbox to release
  Returns:	  -3: process has been zap�d.
			  -1: the mailboxID is not a mailbox that is in use.
			   0: successful completion.
  ----------------------------------------------------------------------- */
int MboxRelease(int mailboxID) {
	// Make sure in Kernel mode
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	disableInterrupts();

	//Check caller is zapped 
	if (isZapped()) {
		if (DEBUG2 && debugflagRelease) {
			USLOSS_Console("MboxRelease(): Given mailbox has been ZAPPED.\n");
		}
		return -3;
	}

	//Check if mailbox has been released
	if (MailBoxTable[mailboxID % MAXMBOX].mboxID == -1) {
		if (DEBUG2 && debugflagRelease) {
			USLOSS_Console("MboxRelease(): Given mailbox has been released.\n");
		}
		return -1;
	}

	// Find the mailbox 
	mailbox *target = &MailBoxTable[mailboxID % MAXMBOX];
	
	// some extra checking
	if (target == NULL) {
		if (DEBUG2 && debugflagRelease) {
			USLOSS_Console("-> MboxRelease(): target is NULL at boxID %d\n", target->mboxID);
		}
		return -1;
	}

	// Unblock on all the processes being blocked by send and receive 
	// unblock Receive 
	mboxProcPtr waitingReceiver;
	while (target->blockedReceivePrt != NULL) {
		// get the waiting receiver
		waitingReceiver = target->blockedReceivePrt;

		// remove and reset pointer in waiting receiver
		target->blockedReceivePrt = waitingReceiver->nextMboxProc;
		waitingReceiver->nextMboxProc = NULL;

		if (DEBUG2 && debugflagRelease) {
			USLOSS_Console("-> MboxRelease():Did removed processID %d at boxID %d\n", waitingReceiver->processPID, target->mboxID);
		}

		// unblocked the calling receiver`s process 
		unblockProc(waitingReceiver->processPID);
		disableInterrupts();
	}

	// Unblocked Send
	mboxProcPtr waitingSend;
	while (target->blockedSendPrt != NULL) {
		// get the waiting receiver
		waitingSend = target->blockedSendPrt;

		// remove and reset pointer in waiting receiver
		target->blockedSendPrt = waitingSend->nextMboxProc;
		waitingSend->nextMboxProc = NULL;

		if (DEBUG2 && debugflagRelease) {
			USLOSS_Console("-> MboxRelease():Did removed processID %d at boxID %d\n", waitingReceiver->processPID, target->mboxID);
		}

		// unblocked the calling receiver`s process 
		unblockProc(waitingSend->processPID);
		disableInterrupts();
	}


	// delete all the slot
	while (target->slots !=NULL){
		initMailSlot(target->slots);
		target->numMessages = target->numMessages - 1;
		if (target->numMessages < 0) {
			USLOSS_Console("-> MboxRelease():ERROR number of mail slot in use is %d, at boxID %d\n", target->numMessages, target->mboxID);
		}
		target->slots = target->slots->nextSlot;
	}

	// delete the mailbox
	if (DEBUG2 && debugflagRelease) {
		USLOSS_Console("-> MboxRelease():Did removed boxID %d\n", target->mboxID);
	}
	initMailBox(target);

	// decreasing total number of mail boxes
	numMailBoxes--;
	enableInterrupts(); 
	return 0;

}


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
	// Make sure in Kernel mode
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	disableInterrupts();
	// variables needed .TP
	mailbox *aBox = &MailBoxTable[mbox_id%MAXMBOX]; // location of the current mail box 
	mboxProc aBoxProc; // new mail box process which is in blocked list 
	
	/* TP
	-1: illegal values given as arguments; or, message being 
	received is too large for receiver�s buffer. 
	No data is copied in this case.
	*/
	if (msg_size > MAX_MESSAGE) { 
		if (DEBUG2 && debugflag2) {
			USLOSS_Console("-> MboxReceive(): message size: %d > MAX_MESSAGE %d\n", msg_size, MAX_MESSAGE);
		}
		enableInterrupts(); 
		return -1; 
	}

	/*TP 
	If the mail box specified has not created 
	return -1
	*/
	if (aBox->mboxID == -1) { 
		if (DEBUG2 && debugflag2) {
			USLOSS_Console("-> MboxReceive(): Inactive mail box. boxPID:%\n", mbox_id%MAXMBOX);
		}
		enableInterrupts();
		return -1;
	}

	/*TP
	Case1:  If there is no message in the mailbox, the calling process is blocked until a message has
	been received.
	*/
	if (aBox->numMessages == 0) {
		if (DEBUG2 && debugflag2) {
			USLOSS_Console("-> MboxReceive(): empty mail box.Blocking calling process PID %d. Number of slots in use in current mail box: %d\n", getpid(), aBox->numMessages);
		}
		// extra checking 
		if (aBox->slots == NULL) {
			if (DEBUG2 && debugflag2) {
				USLOSS_Console("-> MboxReceive(): Empty mail box and there is no slot created.Blocking calling process PID %d. Mail box ID: %d. \n", getpid(), aBox->mboxID);
			}
					
			//  Create new mail box  process 
			aBoxProc.processPID = getpid();
			aBoxProc.msgSize = msg_size;
			aBoxProc.msgPtr = msg_ptr;
			aBoxProc.nextMboxProc = NULL;

			// Adding the new mail box process to the end of block receiver list to wait for sender
			if (aBox->blockedReceivePrt == NULL) {
				aBox->blockedReceivePrt = &aBoxProc;
				if (DEBUG2 && debugflag2) {
					USLOSS_Console("->-> MboxReceive():  First new mail process successful added. Mail box ID: %d, Message size:%d \n", aBox->mboxID, aBoxProc.msgSize);
				}
			}
			else {
				mboxProcPtr temp = aBox->blockedReceivePrt;
				while (temp->nextMboxProc != NULL){
					temp = temp->nextMboxProc;
				}
				temp = &aBoxProc;
				if (DEBUG2 && debugflag2) {
					USLOSS_Console("->-> MboxReceive():  New mail process successful added. Mail box ID: %d\n", aBox->mboxID);
				}
			}
			
			// block the calling process 
			blockMe(EMPTY_BOX);
			disableInterrupts();

			//Check if process was zapped
			if (isZapped()) {
				USLOSS_Console("MboxSend(): Process was zapped while sending.\n");
				enableInterrupts();
				return -3;
			}
			
			// return message size
			if (DEBUG2 && debugflag2) {
				USLOSS_Console("->-> MboxReceive(): Return msg_size after blockme() at mail box ID: %d\n", aBox->mboxID);
			}
			enableInterrupts();
			return aBoxProc.msgSize;
		}
	} else { // There is available message in the mailbox  
		if (DEBUG2 && debugflag2) {
			USLOSS_Console("-> MboxReceive(): There is available message. Calling process PID %d. Number of slots in use in current mail box: %d\n", getpid(), aBox->numMessages);
		}

		// extra checking 
		if (aBox->slots == NULL) {
			//if (DEBUG2 && debugflag2) {
				USLOSS_Console("->-> MboxReceive(): There is no slot created but messages in use != 0.Blocking calling process PID %d. Mail box ID: %d. \n", getpid(), aBox->mboxID);
			//}
		}

		// return the first message in the slot 
		slotPtr aSlotPtr = aBox->slots;

		// Check for empty slot
		if (aSlotPtr->status == SLOTEMPTY) {
			//if (DEBUG2 && debugflag2) {
				USLOSS_Console("->-> MboxReceive(): try to send an empty SLOT. Calling process PID %d. Mail box ID: %d. \n", getpid(), aBox->mboxID);
			//}
			enableInterrupts(); 
			return -1;
		}

		// Copy information from the slot to *msg_ptr argument
		memcpy(msg_ptr, aSlotPtr->message, aSlotPtr->msgSize);

		// store msg_size for return value 
		int result = aSlotPtr->msgSize;

		// Remove the slot from current mailbox`s slots list
		aBox->slots = aSlotPtr->nextSlot;
		

		// Free the slot in MailSlotTable  
		aSlotPtr->mboxID   = -1;
		aSlotPtr->msgSize  = -1;
		aSlotPtr->status   = SLOTEMPTY;
		aSlotPtr->nextSlot = NULL;
		
		// update numMessage of the current mailbox 
		aBox->numMessages = aBox->numMessages - 1;
		filledSlots       = filledSlots - 1;

		/*
		After the slot above has been free
		If there is blocked send mail box process not added to 
		the slots list due to it is full.
		- Take that process off the blocked send list
		- Use it and the mail slot just free to create new mail slot 
		and add that slot to the END the slots list.
		- No need to update on MailSlotTable. 
		because we reuse free slot above so the information of 
		the new created slot will be auto updated in the MailSlotTable[]
		- update numMessage of the current mailbox and filledSlots 
		- unblock the process called send
		*/
		if (aBox->blockedSendPrt != NULL) {
			if (DEBUG2 && debugflag2) {
				USLOSS_Console("->-> MboxReceive(): Adding bocked send message to Slots @ Mail box ID: %d. \n", aBox->mboxID);
			}
			// some extra checking 
			if (aBox->numMessages != aBox->totalSlots -1) { // -1 due to the remove slot above 
				USLOSS_Console("->-> MboxReceive(): ERROR: Adding bock send to Slots @ Mail box ID: %d. \n", aBox->mboxID);
				USLOSS_Console("->-> MboxReceive(): But NumMessage: %d != Max number of slots allowed: %d\n", aBox->numMessages, aBox->totalSlots);
			}

			mboxProcPtr aBlockedSendMsg = aBox->blockedSendPrt;
			aBox->blockedSendPrt = aBlockedSendMsg->nextMboxProc;

			// some debug outputs 
			if (aBox->blockedSendPrt == NULL) {
				if (DEBUG2 && debugflag2) {
					USLOSS_Console("->-> MboxReceive(): ERROR: blockedSentPtr is NULL2 Mail box ID: %d. \n", aBlockedSendMsg->nextMboxProc, aBox->mboxID);
				}
			}
			
			aSlotPtr->mboxID = mbox_id;
			aSlotPtr->status = SLOTFULL;
			aSlotPtr->msgSize = aBlockedSendMsg->msgSize;
			memcpy(aSlotPtr->message, aBlockedSendMsg->msgPtr, aBlockedSendMsg->msgSize);
			if (aBox->slots == NULL) {
				aBox->slots = aSlotPtr;
			}
			else {
				slotPtr temp = aBox->slots;
				while (temp->nextSlot != NULL) {
					temp = temp->nextSlot;
				}
				temp->nextSlot = aSlotPtr;
			}
			

			aBox->numMessages++;
			filledSlots++;

			unblockProc(aBlockedSendMsg->processPID);
		}
				
		// return msg size
		if (DEBUG2 && debugflag2) {
			USLOSS_Console("->-> MboxReceive(): Return msg_size without calling block me at mail box ID: %d\n", aBox->mboxID);
		}
		enableInterrupts(); 
		return result;
	} // end Case1

	if (DEBUG2 && debugflag2) {
		USLOSS_Console("-> MboxReceive(): FIXME: we should not see this message! process PID: %d. Mail box ID: %d\n", getpid(), aBox->mboxID);
	}

	return -99; // get away from the warning
    // memcpy(msg_ptr, MailBoxTable[mbox_id % MAXMBOX].slots->message, msg_size);
    // return MailBoxTable[mbox_id % MAXMBOX].slots->msgSize;
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

/* ------------------------------------------------------------------------
   Initializing MailBox and MailSlot struct. 
   ------------------------------------------------------------------------ */
void initMailBox(mailbox *m){
    m->mboxID = -1;
    m->numMessages = 0;
    m->totalSlots = 0;
    m->slotSize = 0;
    m->slots = NULL;
	m->blockedReceivePrt = NULL;
}

void initMailSlot(slotPtr s){
    s->mboxID = -1;
    s->msgSize = -1;
    s->status = SLOTEMPTY;
    s->nextSlot = NULL;
}
/* ------------------------------------------------------------------------
Name - enableInterrupts()
Purpose - Turn the interrupts ON iff we are in kernel mode
if not in kernel mode, print an error message and halt USLOSS
Parameters - None
Returns - None
Side Effects -
----------------------------------------------------------------------- */
void disableInterrupts()
{
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	else {
		USLOSS_PsrSet(USLOSS_PsrGet() ^ (USLOSS_PsrGet() & 0x2));
	}
} /* disableInterrupts */

  /* ------------------------------------------------------------------------
  Name - enableInterrupts()
  Purpose - Turn the interrupts ON iff we are in kernel mode
  if not in kernel mode, print an error message and halt USLOSS
  Parameters - None
  Returns - None
  Side Effects -
  ----------------------------------------------------------------------- */
void enableInterrupts()
{
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("disableInterrupts: in user mode. Halting...\n");
		USLOSS_Halt(1);
	}
	else {
		USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);
	}
} /* enableInterrupts */
/* ------------------------------------------------------------------------
FIX ME:  warning: ignoring return value of �USLOSS_PsrSet�, 
declared with attribute warn_unused_result [-Wunused-result]

STOPHERE: test05: 
	-> everything working ok except  return from receiver  == 100? 
------------------------------------------------------------------------ */
