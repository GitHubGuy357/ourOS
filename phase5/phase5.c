/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */


#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>

#include <vm.h>
#include <string.h>
#include <stdio.h>

int     debugVal = 3; // 0 == off to 3 == most debug info

/*****************************************************
*             Globals
*****************************************************/
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
static Process processes[MAXPROC];
FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;
void *vmRegion;
char buf[MAXARG];
char name[MAXNAME];
int i;

/*****************************************************
*             ProtoTypes
*****************************************************/
void FaultHandler(int type, void * offset);
void vmInit(USLOSS_Sysargs *args);
void vmDestroy(USLOSS_Sysargs *args);
extern  int  start5(char * name); // Had to add for Dr. Homers testcases to be forked in Phase5 code, obviously...
int pDebug(int level, char *fmt, ...);
static int Pager(char *buf);

/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int start4(char *arg) {
    int pid;
    int result;
    int status;

	pDebug(3,"start4(): start\n");
    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy; 
	
	pDebug(3,"start4(): Before spawn testcases start5\n");
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
	pDebug(3,"start4(): Before wait\n");
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */


/***********************************************************************
 *                        SYSTEM CALLS
 **********************************************************************/


/*----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *arg1: number of mappings the MMU should hold
 *arg2: number of virtual pages to use
 *arg3: number of physical page frames to use
 *arg4: number of pager daemons
 * 
 *Output
 *arg1: address of the first byte in the VM region
 *arg4: -1 if illegal values are given as input; -2 if the VM region has already been
 *		initialized; 0 otherwise.
 *----------------------------------------------------------------------
 */
void vmInit(USLOSS_Sysargs *args){
	pDebug(3,"vmInit(): start\n");
    CheckMode();
	void* returnVal = vmInitReal((long)args->arg1,(long)args->arg2,(long)args->arg3,(long)args->arg4);
	args->arg1 = vmRegion;
	args->arg4 = returnVal;
	pDebug(3,"vmInit(): end\n");
} /* vmInit */

/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.

 * USLOSS_MMU_OK No error.
 * USLOSS_MMU_ERR_OFF MMU has not been initialized.
 * USLOSS_MMU_ERR_ON MMU has already been initialized.
 * USLOSS_MMU_ERR_PAGE Invalid page number.
 * USLOSS_MMU_ERR_FRAME Invalid frame number.
 * USLOSS_MMU_ERR_PROT Invalid protection.
 * USLOSS_MMU_ERR_TAG Invalid tag.
 * USLOSS_MMU_ERR_REMAP Mapping with same tag & page already exists.
 * USLOSS_MMU_ERR_NOMAP Mapping not found.
 * USLOSS_MMU_ERR_ACC Invalid access bits.
 * USLOSS_MMU_ERR_MAPS Too many mappings.
 * USLOSS_MMU_ERR_MODE Operation is invalid in the current MMU mode.
 *----------------------------------------------------------------------
 */
void *vmInitReal(int mappings, int pages, int frames, int pagers){
   pDebug(3,"vmInitReal(): start\n");
   int status;
   int dummy = pages;
   int pid;

   CheckMode();
   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
   if (status != USLOSS_MMU_OK) {
	  USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
	  abort();
   }
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

   /*
	* Initialize page tables.
    */

   /* 
    * Create the fault mailbox.
    */

   /*
    * Fork the pagers.
    */
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(buf, "%d", i);
		sprintf(name, "Pager %d", i);
        pid = fork1(name, Pager, buf, USLOSS_MIN_STACK*2, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create pager driver %d\n", i);
            USLOSS_Halt(1);
        }
    }
   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   /*
    * Initialize other vmStats fields.
    */
   pDebug(3,"vmInitReal(): end\n");
   return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */

/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

void vmDestroy(USLOSS_Sysargs *args){
   pDebug(3,"vmDestroy(): start\n");
   CheckMode();
   pDebug(3,"vmDestroy(): end\n");
} /* vmDestroy */




/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void vmDestroyReal(void){
   pDebug(3,"vmDestroyReal(): start\n");
   CheckMode();
   USLOSS_MmuDone();
   /*
    * Kill the pagers here.
    */
   /* 
    * Print vm statistics.
    */
   USLOSS_Console("vmStats:\n");
   USLOSS_Console("pages: %d\n", vmStats.pages);
   USLOSS_Console("frames: %d\n", vmStats.frames);
   USLOSS_Console("blocks: %d\n", vmStats.blocks);
   /* and so on... */
   pDebug(3,"vmDestroyReal(): end\n");
} /* vmDestroyReal */



/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void PrintStats(void){
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
//static void FaultHandler(int type /* MMU_INT */, int arg  /* Offset within VM region */){
void FaultHandler(int type /* MMU_INT */, void * arg  /* Offset within VM region */){
   pDebug(3,"FaultHandler(): start\n");
   int cause;

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
	pDebug(3,"FaultHandler(): end\n");
} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int Pager(char *buf){
	pDebug(3,"Pager(): end\n");
    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
    }
	pDebug(3,"Pager(): start\n");
    return 0;
} /* Pager */


/*************************************************************************
*
*                           UTILITIES
*
*************************************************************************/

/*****************************************************************************
 *     pDebug - Outputs a printf-style formatted string to stdout
 *****************************************************************************/
int pDebug(int level, char *fmt, ...) {
	 va_list args;
	 va_start(args, fmt);
     if(debugVal >= level)
        USLOSS_VConsole(fmt,args);
	 return 1;
} /* pDebug */
