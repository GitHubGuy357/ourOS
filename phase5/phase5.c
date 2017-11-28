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
//static Process processes[MAXPROC];
FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;
void *vmRegion;
char buf[MAXARG];
char name[MAXNAME];
int i;
Process ProcTable5[MAXPROC];
FTE *FrameTable; // Pointer as we do not know the amount of frames ahead of time. This is why we finally use malloc!
FaultMsg FaultTable[MAXPROC];
DiskStat Disk;
int fault_mbox;
int VMInitialized = 0;


/*****************************************************
*             ProtoTypes
*****************************************************/
void dp5();
void FaultHandler(int type, void * offset);
void vmInit(USLOSS_Sysargs *args);
void vmDestroy(USLOSS_Sysargs *args);
extern  int  start5(char * name); // Had to add for Dr. Homers testcases to be forked in Phase5 code, obviously...
int pDebug(int level, char *fmt, ...);
static int Pager(char *buf);
void putUserMode();
int check_kernel_mode(char *procName);
void pMem(void* buff,int len);

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
    systemCallVec[SYS_VMINIT]          = vmInit;
    systemCallVec[SYS_VMDESTROY]       = vmDestroy; 
	
	
	DiskSize(1, &Disk.sectSize, &Disk.numSects, &Disk.numTracks);
		
	pDebug(3," <- start4(): Before spawn testcases start5\n");
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
	pDebug(3," <- start4(): Before wait\n");
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
 * Input
 *	arg1: number of mappings the MMU should hold
 *	arg2: number of virtual pages to use
 *	arg3: number of physical page frames to use
 *	arg4: number of pager daemons
 * 
 * Output
 *	arg1: address of the first byte in the VM region
 *	arg4: -1 if illegal values are given as input; -2 if the VM region has already been
 *		initialized; 0 otherwise.
 *----------------------------------------------------------------------
 */
void vmInit(USLOSS_Sysargs *args){
	pDebug(3," <- vmInit(): start\n");
    CheckMode();
	void* returnVal = vmInitReal((long)args->arg1,(long)args->arg2,(long)args->arg3,(long)args->arg4);
	args->arg1 = returnVal;
	vmRegion = returnVal;
	
	// If return val is < 0 MMU has been initialized or illegal values were given. Else return 0 for success.
	if(returnVal < 0)
		args->arg4 = returnVal;
	else
		args->arg4 = (void*)(long)0;
	
	pDebug(3," <- vmInit(): end\n");
	putUserMode();
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
 * 
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
   pDebug(3," <- vmInitReal(): start\n");
   int status;
   int dummy;
   int pid;

   // Check for invalid arguments, return -1 if invalid
   if(pagers > MAXPAGERS)
	   return (void*)-1;
   
   // Check if VM has allready been initialized
   if(VMInitialized == 1)
	   return (void*)-2;
   
   // Check if we are in kernal?
   CheckMode();
   
   // Initialize MMU Unit
   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
   if (status != USLOSS_MMU_OK) {
	  USLOSS_Console(" <- vmInitReal: couldn't initialize MMU, status %d\n", status);
	  abort();
   }
   
   // Set Systemcall vec to point to our FaultHandler
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

   /*
	* Initialize page tables.
    */
	//status = USLOSS_MmuInit(mappings,pages,frames,USLOSS_MMU_MODE_PAGETABLE);

   /* 
    * Create the fault mailbox.
    */
	fault_mbox = MboxCreate(pagers,MAX_MESSAGE);
	
	// Create All 50 possible processes
    for (i=0; i<MAXPROC; i++){
	    ProcTable5[i].numPages = pages;
		ProcTable5[i].pageTable =  NULL;        
		ProcTable5[i].privateMBox = MboxCreate(0,0);
		ProcTable5[i].pid = i;
		FaultTable[i].replyMbox = -1;
    }
	
	// Initialize frame table
	FrameTable = malloc(sizeof(FTE)*frames);
	
	// Initialize each frame requested
	for (i=0;i<frames;i++){
		FrameTable[i].state = F_UNUSED;
		FrameTable[i].frame = i;
		FrameTable[i].page = -1;
		FrameTable[i].procPID = -1;
		//temp->next = &FrameTable[i+1];
	}

	// Get Disk info, and set each block to D_UNUSED
	Disk.blockCount = (Disk.sectSize * Disk.numSects * Disk.numTracks) / USLOSS_MmuPageSize();
	Disk.blockStatus = malloc(Disk.blockCount * sizeof(int));
	Disk.blocksInTrack = Disk.numTracks / Disk.blockCount;
	Disk.blocksInSector = USLOSS_MmuPageSize() / Disk.numSects;
	for(i=0;i<Disk.blockCount;i++)
		Disk.blockStatus[i] = D_UNUSED;
	
	// Update vmStats
	vmStats.diskBlocks = Disk.blockCount;
    vmStats.freeDiskBlocks = Disk.blockCount;
   
   /*
    * Fork the pagers.
    */
    for (i = 0; i < pagers; i++) {
        sprintf(buf, "%d", i);
		sprintf(name, "Pager %d", i);
        pid = fork1(name, Pager, buf, USLOSS_MIN_STACK*2, PAGER_PRIORITY);
        if (pid < 0) {
            USLOSS_Console(" <- vmInitReal(): Can't create pager driver %d\n", i);
            USLOSS_Halt(1);
        }
    }
	dumpProcesses();
   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   vmStats.new = 0;
   
   /*
    * Initialize other vmStats fields.
    */
   pDebug(3," <- vmInitReal(): end\n");
   
   // Bookeeping, VM Region has been created!
   VMInitialized = 1;
     
   return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */

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
   pDebug(3," <- FaultHandler(): start\n");
   int cause;

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
	FaultTable[getpid()%MAXPROC].pid = getpid();
	FaultTable[getpid()%MAXPROC].addr = arg;
	if(FaultTable[getpid()%MAXPROC].replyMbox == -1)
		FaultTable[getpid()%MAXPROC].replyMbox = MboxCreate(0,MAX_MESSAGE);

	pDebug(1," <- FaultHandler(): FaultTable @mem[%p] id=%d, replyMbox=%d offset[%p]\n",&FaultTable[getpid()%MAXPROC], FaultTable[getpid()%MAXPROC].pid,FaultTable[getpid()%MAXPROC].replyMbox, arg);

	// Send fault object to Pager
	MboxSend(fault_mbox,&FaultTable[getpid()%MAXPROC],sizeof(FaultMsg));
	
	// Block waiting for fault to be resolved
	pDebug(1," <- FaultHandler(): Before Recv on FaultTable replyMbox\n");
	MboxReceive(FaultTable[getpid()%MAXPROC].replyMbox,NULL,0);
	pDebug(3," <- FaultHandler(): end\n");
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
 * p ((FaultMsg*)recv_buff).pid
 * p ((FaultMsg*)test).pid
 *----------------------------------------------------------------------
 */
static int Pager(char *buf){
	pDebug(3," <- Pager(): start\n");
	
	// Pager variables
	void *recv_buff[MAX_MESSAGE];
	procPtr tempProc;
	PTE *pagePtr = NULL;
	int map_result;

	
	// Loop until
    while(1) {
		pDebug(3," <- Pager(): Before Block awaiting fault...\n");
		MboxReceive(fault_mbox,recv_buff,sizeof(FaultMsg));
		FaultMsg *fm = ((FaultMsg*)recv_buff);
		tempProc = &ProcTable5[fm->pid%MAXPROC];
		pDebug(1, " <- Pager(): Fault Received... @mem[%p], pid[%d.%d], offset_arrg[%d.%d], reply_mboxID[%d.%d] pager_buf[%s] \n",fm,fm->pid,FaultTable[fm->pid%MAXPROC].pid,fm->addr,FaultTable[fm->pid%MAXPROC].addr,fm->replyMbox, FaultTable[fm->pid%MAXPROC].replyMbox,buf);
		
	    /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
        /* If there isn't one then use clock algorithm to */
        /* replace a page (perhaps write to disk) */
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
		
		// Set page to requested page, dont forget to check if INUSE
		pagePtr = &tempProc->pageTable[(long)(void*)fm->addr / USLOSS_MmuPageSize()];
		
		// Find Frame
		pDebug(3," <- Pager(): Searching for frame...");
		for (i=0;i<tempProc->numPages;i++){
			if(FrameTable[i].state == F_UNUSED){
				vmStats.new++;
				pagePtr->state = INMEM;
				pagePtr->frame = FrameTable[i].frame;
				FrameTable[i].state = F_INUSE;
				FrameTable[i].page = (long)fm->addr;
				FrameTable[i].frame = i;
				pDebug(3,"found @ %d.\n",pagePtr->frame);
				        // map page 0 to frame so we can write to it later
				map_result = USLOSS_MmuMap(TAG, 0, FrameTable[i].frame, USLOSS_MMU_PROT_RW);
				map_result = map_result;
				pDebug(1," <- Pager(): calling MmuMap with (tag=[%d],pagePtr->page=[%d],FrameTable[i].frame=[%d], access=[%d])\n",(int)TAG,0,FrameTable[i].frame,USLOSS_MMU_PROT_RW);
				memset(vmRegion, 0, USLOSS_MmuPageSize());
			 
			    // Un-map pager
				//USLOSS_MmuSetAccess(FrameTable[i].frame, 0); // set page as clean
				//USLOSS_MmuUnmap(TAG, 0); // unmap page
				break;
			}
		}

		// Page fault handled, wake up faulting process
		MboxSend(fm->replyMbox,NULL,0);
    }
	pDebug(3," <- Pager(): end\n");
    return 0;
} /* Pager */

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
   pDebug(3," <- vmDestroy(): start\n");
   CheckMode();
   pDebug(3," <- vmDestroy(): end\n");
   putUserMode();
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
   int status = -1;
   pDebug(3," <- vmDestroyReal(): start\n");
   CheckMode();
   status = USLOSS_MmuDone();
   status=status; //Stop warnings
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
   pDebug(3," <- vmDestroyReal(): end\n");
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

/*************************************************************************
*
*                           UTILITIES
*
*************************************************************************/

/*****************************************************************************
 *     putUserMode - puts the OS into usermode with the appropriate call to 
 *		psrSet
 *****************************************************************************/
void putUserMode(){
	int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
	pDebug(3," <- putUserMode(): Result = %d\n",result);
}

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

/*************************************************************************
*   check_kernel_mode - Checks if the PsrBit is set to kernal mode
*   	when called by process, if not halts.
*************************************************************************/
int check_kernel_mode(char *procName){
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        USLOSS_Console("Process [%s] pid[%d] to call a function in user mode...halting.\n",procName, getpid());
        USLOSS_Halt(1);
        return 0;
    }else
        return 1;
} /* check_kernel_mode */

void pMem(void* buff,int len){
	for (int i = 0; i < len; i++)
		printf("%02x", ((unsigned char *) buff) [i]);
}

/*****************************************************************************
 *     dp5 - Used to output a formatted version of the process table to the 
 *		     usloss console
 *****************************************************************************/
void dp5(){
    USLOSS_Console("\n--------------PROCESS TABLE--------------\n");
    USLOSS_Console(" PID  numPages  pageTable    privateMbox\n");
    USLOSS_Console("-----------------------------------------\n");

	for( i= 0; i< MAXPROC;i++){
		if(ProcTable5[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %s[%-2d] %-5s[%10p] %-0s[%-2d]\n","",ProcTable5[i].pid,"",ProcTable5[i].numPages,"",ProcTable5[i].pageTable,"",ProcTable5[i].privateMBox);
		}	  
	}
    USLOSS_Console("-------------------------------------------\n");    
}
