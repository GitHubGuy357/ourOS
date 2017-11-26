/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

//#define PHASE_3        //currently defined by -DPHASE_3 added to compiler flags

#include <string.h>
#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase5.h>
#include <libuser.h>

#define CHECKMODE {						\
	if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { 				\
	    USLOSS_Console("Trying to invoke syscall from kernel\n");	\
	    USLOSS_Halt(1);						\
	}							\
}

/* PROTOTYPES */
	extern int pDebug(int level, char *fmt, ...);

/*
 *  Routine:  Mbox_Create
 *
 *  Description: This is the call entry point to create a new mail box.
 *
 *  Arguments:    int   numslots -- number of mailbox slots
 *                int   slotsize -- size of the mailbox buffer
 *                int  *mboxID   -- pointer to output value
 *                (output value: id of created mailbox)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Create(int numslots, int slotsize, int *mboxID)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_MBOXCREATE;
    sysArg.arg1 = (void *) (long) numslots;
    sysArg.arg2 = (void *) (long) slotsize;
    USLOSS_Syscall(&sysArg);
    *mboxID = (int) (long) sysArg.arg1;
    return (int) (long) sysArg.arg4;
} /* end of Mbox_Create */


/*
 *  Routine:  Mbox_Release
 *
 *  Description: This is the call entry point to release a mailbox
 *
 *  Arguments: int mbox  -- id of the mailbox
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Release(int mboxID)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_MBOXRELEASE;
    sysArg.arg1 = (void *) (long) mboxID;
    USLOSS_Syscall(&sysArg);
    return (int) (long) sysArg.arg4;
} /* end of Mbox_Release */


/*
 *  Routine:  Mbox_Send
 *
 *  Description: This is the call entry point mailbox send.
 *
 *  Arguments:    int mboxID    -- id of the mailbox to send to
 *                int msgSize   -- size of the message
 *                void *msgPtr  -- message to send
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Send(int mboxID, void *msgPtr, int msgSize)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_MBOXSEND;
    sysArg.arg1 = (void *) (long) mboxID;
    sysArg.arg2 = (void *) (long) msgPtr;
    sysArg.arg3 = (void *) (long) msgSize;
    USLOSS_Syscall(&sysArg);
    return (int) (long) sysArg.arg4;
} /* end of Mbox_Send */


/*
 *  Routine:  Mbox_Receive
 *
 *  Description: This is the call entry point for terminal input.
 *
 *  Arguments:    int mboxID    -- id of the mailbox to receive from
 *                int msgSize   -- size of the message
 *                void *msgPtr  -- message to receive
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Receive(int mboxID, void *msgPtr, int msgSize)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_MBOXRECEIVE;
    sysArg.arg1 = (void *) (long) mboxID;
    sysArg.arg2 = (void *) (long) msgPtr;
    sysArg.arg3 = (void *) (long) msgSize;
    USLOSS_Syscall( &sysArg );
        /*
         * This doesn't belong here. The copy should by done by the
         * system call.
         */
        if ( (int) (long) sysArg.arg4 == -1 )
                return (int) (long) sysArg.arg4;
        memcpy( (char*)msgPtr, (char*)sysArg.arg2, (int) (long) sysArg.arg3);
        return 0;

} /* end of Mbox_Receive */


/*
 *  Routine:  Mbox_CondSend
 *
 *  Description: This is the call entry point mailbox conditional send.
 *
 *  Arguments:    int mboxID    -- id of the mailbox to send to
 *                int msgSize   -- size of the message
 *                void *msgPtr  -- message to send
 *
 *  Return Value: 0 means success, -1 means error occurs, 1 means mailbox
 *                was full
 *
 */
int Mbox_CondSend(int mboxID, void *msgPtr, int msgSize)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_MBOXCONDSEND;
    sysArg.arg1 = (void *) (long) mboxID;
    sysArg.arg2 = (void *) (long) msgPtr;
    sysArg.arg3 = (void *) (long) msgSize;
    USLOSS_Syscall(&sysArg);
    return ((int) (long) sysArg.arg4);
} /* end of Mbox_CondSend */


/*
 *  Routine:  Mbox_CondReceive
 *
 *  Description: This is the call entry point mailbox conditional
 *               receive.
 *
 *  Arguments:    int mboxID    -- id of the mailbox to receive from
 *                int msgSize   -- size of the message
 *                void *msgPtr  -- message to receive
 *
 *  Return Value: 0 means success, -1 means error occurs, 1 means no
 *                message was available
 *
 */
int Mbox_CondReceive(int mboxID, void *msgPtr, int msgSize)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_MBOXCONDRECEIVE;
    sysArg.arg1 = (void *) (long) mboxID;
    sysArg.arg2 = (void *) (long) msgPtr;
    sysArg.arg3 = (void *) (long) msgSize;
    USLOSS_Syscall( &sysArg );
    return ((int) (long) sysArg.arg4);
} /* end of Mbox_CondReceive */


/*
 *  Routine:  VmInit
 *
 *  Description: Initializes the virtual memory system.
 *
 *  Arguments:    int mappings -- # of mappings in the MMU
 *                int pages -- # pages in the VM region
 *                int frames -- # physical page frames
 *                int pagers -- # pagers to use
 *
 *  Return Value: address of VM region, NULL if there was an error
 *
 */
int VmInit(int mappings, int pages, int frames, int pagers, void **region)
{
    USLOSS_Sysargs sysArg;
    int result;

    CHECKMODE;

    sysArg.number = SYS_VMINIT;
    sysArg.arg1 = (void *) (long) mappings;
    sysArg.arg2 = (void *) (long) pages;
    sysArg.arg3 = (void *) (long) frames;
    sysArg.arg4 = (void *) (long) pagers;

    USLOSS_Syscall(&sysArg);

    *region = sysArg.arg1;  // return address of VM Region

    result = (int) (long) sysArg.arg4;

    if (sysArg.arg4 == 0) {
        return 0;
    } else {
        return result;
    }
} /* VmInit */


/*
 *  Routine:  VmDestroy
 *
 *  Description: Tears down the VM system
 *
 *  Arguments:
 *
 *  Return Value:
 *
 */

int VmDestroy(void) {
    USLOSS_Sysargs     sysArg;

    CHECKMODE;
    sysArg.number = SYS_VMDESTROY;
    USLOSS_Syscall(&sysArg);
    return (int) (long) sysArg.arg1;
} /* VmDestroy */

/* end libuser.c */
