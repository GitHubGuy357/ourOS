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
#include <providedPrototypes.h>

#include <vm.h>
#include <string.h>
#include <stdio.h>

int     debugVal = 0; // 0 == off to 3 == most debug info

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
int Pagers[MAXPAGERS] = {-1, -1, -1, -1};
char mmu_results[12][40] = {"Everything hunky-dory","MMU not enabled","MMU already initialized", "Invalid page number","Invalid frame number","Invalid protection","Invalid tag","Page already mapped","Page not mapped","Invalid access bits","Too many mappings","Invalid MMU mode"};
char page_results[4][10] = {"UNUSED","INMEM","INDISK","INBOTH"};
char frame_results[2][10] = {"F_UNUSED","F_USED"};
char disk_results[2][10] = {"D_UNUSED","D_USED"};
/*****************************************************
*             ProtoTypes
*****************************************************/
void dp5();
char* get_r(int id);
void FaultHandler(int type, void * offset);
void vmInit(USLOSS_Sysargs *args);
void vmDestroy(USLOSS_Sysargs *args);
extern  int  start5(char * name); // Had to add for Dr. Homers testcases to be forked in Phase5 code, obviously...
int pDebug(int level, char *fmt, ...);
static int Pager(char *buf);
void putUserMode();
int check_kernel_mode(char *procName);
void pMem(void* buff,int len);
void PrintStats(void);
void printPages(PTE *pageTable);


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
	pDebug(2," <- vmInitReal(): Created fault_mbox [%d] with %d pagers\n",fault_mbox,pagers);
	
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
		FrameTable[i].isLocked = 0;
	}

	// Get Disk info, and set each block to D_UNUSED
	diskSizeReal(1, &Disk.sectSize, &Disk.numSects, &Disk.numTracks);
	pDebug(2," <- vmInitReal(): Using Disk[1], [sectSize=%d numSects=%d numTracks=%d]\n",Disk.sectSize, Disk.numSects,Disk.numTracks);
	Disk.blockCount = (Disk.sectSize * Disk.numSects * Disk.numTracks) / USLOSS_MmuPageSize();
	Disk.blockStatus = malloc(Disk.blockCount * sizeof(int));
	Disk.blocksInTrack = Disk.numTracks / Disk.blockCount;
	Disk.blocksInSector = USLOSS_MmuPageSize() / Disk.numSects;
	for(i=0;i<Disk.blockCount;i++)
		Disk.blockStatus[i] = D_UNUSED;
  
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
		pDebug(2," <- vmInitReal(): Forking Pager[%d] @ pid[%d]\n",i,pid);
		Pagers[i] = pid;
    }
	
	if(debugVal>2)dumpProcesses();
	
   /*
    * Zero out, then initialize, the vmStats structure
    */
	// Update vmStats
    memset((char *) &vmStats, 0, sizeof(VmStats));
    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.new = 0;
	vmStats.diskBlocks = Disk.blockCount;
    vmStats.freeDiskBlocks = Disk.blockCount;
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
   pDebug(2," <- FaultHandler(): start\n");
   int cause;
   int pid;
 //  procPtr tempProc;
   int reply_buff;
   
   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
	pid = getpid()%MAXPROC;
	//tempProc = &ProcTable5[pid];
	FaultTable[pid].pid = pid;
	FaultTable[pid].addr = arg;
	if(FaultTable[pid].replyMbox == -1)
		FaultTable[pid].replyMbox = MboxCreate(0,MAX_MESSAGE);

	pDebug(1," <- FaultHandler(): Sending fault...pid=[%d] offset=[%d] to Pager\n",pid,(long)arg/USLOSS_MmuPageSize()); 
	
	// Send fault object to Pager
	MboxSend(fault_mbox,&FaultTable[pid],sizeof(FaultMsg));
	
	// Block waiting for fault to be resolved and Receive frame from pager
	pDebug(1," <- FaultHandler(): Before Recv on FaultTable replyMbox by pid[%d]\n",pid);
	MboxReceive(FaultTable[pid].replyMbox,&reply_buff,sizeof(int));
	pDebug(1," <- FaultHandler(): After Recv on FaultTable replyMbox by pid[%d], reply_buff = [%d]\n",pid,reply_buff);
	
	// Find a page in process we can map the frame
	//PTE *procPage = &tempProc->pageTable[(long)(void*)arg / USLOSS_MmuPageSize()];
	
	// Set frame to page, dont forget to check if INDISK if Pager took frame from disk
	//if(procPage->state == INDISK)
	//	procPage->state = INBOTH;      // See above.
	//else
	//	procPage->state = INMEM; 
    //rocPage->frame = reply_buff;

	// remove lock, or test 10 will fail
	pDebug(1," <- FaultHandler(): Unlocking frame [%d]\n",reply_buff);
	FrameTable[reply_buff].isLocked = 0;
	
	pDebug(2," <- FaultHandler(): end\n");
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
 * #define USLOSS_MMU_OK           0    Everything hunky-dory 
 * #define USLOSS_MMU_ERR_OFF      1    MMU not enabled 
 * #define USLOSS_MMU_ERR_ON       2    MMU already initialized 
 * #define USLOSS_MMU_ERR_PAGE     3    Invalid page number 
 * #define USLOSS_MMU_ERR_FRAME    4    Invalid frame number 
 * #define USLOSS_MMU_ERR_PROT     5    Invalid protection 
 * #define USLOSS_MMU_ERR_TAG      6    Invalid tag 
 * #define USLOSS_MMU_ERR_REMAP    7    Page already mapped 
 * #define USLOSS_MMU_ERR_NOMAP    8    Page not mapped 
 * #define USLOSS_MMU_ERR_ACC      9    Invalid access bits 
 * #define USLOSS_MMU_ERR_MAPS     10   Too many mappings 
 * #define USLOSS_MMU_ERR_MODE     11   Invalid MMU mode 
 *----------------------------------------------------------------------
 */
static int Pager(char *buf){
	pDebug(3," <- Pager(): start\n");
	
	// Pager variables
	void *recv_buff[MAX_MESSAGE];
	int map_result;
	int frame = -1;
	int isFrameReplaced = 0;
	PTE *faultPage;
	
	// Loop until
    while(1) {
		pDebug(3," <- Pager(): Before Block awaiting fault...\n");
		MboxReceive(fault_mbox,recv_buff,sizeof(FaultMsg));
		if (isZapped())
			break; 
		FaultMsg *fm = ((FaultMsg*)recv_buff);
		pDebug(1, " <- Pager(): Fault Received...from pid[%d.%d] @ offset[%d.%d], reply_mboxID[%d.%d] pager_buf[%s] \n",fm->pid,FaultTable[fm->pid%MAXPROC].pid,(long)fm->addr/ USLOSS_MmuPageSize(),(long)FaultTable[fm->pid%MAXPROC].addr/ USLOSS_MmuPageSize(),fm->replyMbox, FaultTable[fm->pid%MAXPROC].replyMbox,buf);
		
		// Process that has faulted
		faultPage = &ProcTable5[(long)fm->pid].pageTable[(long)fm->addr / USLOSS_MmuPageSize()];
		
	    /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */

        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
		
		// Find Frame
		pDebug(1," <- Pager(): Searching for frame...");
		for (frame=0;frame<vmStats.frames;frame++){
			if(FrameTable[frame].state == F_UNUSED && FrameTable[frame].isLocked == 0){
				FrameTable[frame].isLocked = 1;
				pDebug(1," frame [%d] found!\n",frame);
				pDebug(2," <- Pager(): Locking frame [%d]\n",frame);
				// Set frame to page, dont forget to check if INDISK if Pager took frame from disk
				// Find a page in process we can map the frame
				break;
			}
		}
		
		// No Frame Found
		/* If there isn't one then use clock algorithm to */
        /* replace a page (perhaps write to disk) */
		if(frame == vmStats.frames){
			frame = 0;
			int accessBit;
			pDebug(1," frame not found...\n");
			
			while (!isFrameReplaced){
				 map_result = USLOSS_MmuGetAccess(frame, &accessBit); // get access bits
				 if ((accessBit & USLOSS_MMU_REF) == 0) {
					 pDebug(1," <- Pager(): page[%d] is dirty\n",frame);
					 break;
				 }else{
					 pDebug(1," <- Pager(): page[%d] is not dirty\n",frame);
					 ProcTable5[FrameTable[frame].procPID%MAXPROC].pageTable[FrameTable[frame].page].frame = -1;
					 ProcTable5[FrameTable[frame].procPID%MAXPROC].pageTable[FrameTable[frame].page].page = -1;
					 ProcTable5[FrameTable[frame].procPID%MAXPROC].pageTable[FrameTable[frame].page].state = UNUSED; 
					 break;
				 }
				frame = frame+1 % vmStats.frames;
			}
		}
						
		// Temp pager mapping to MMU region so we can memset to 0 OR write frame from disk to vmRegion
		map_result = USLOSS_MmuMap(TAG, 0, FrameTable[frame].frame, USLOSS_MMU_PROT_RW);
		map_result = map_result;
		pDebug(1," <- Pager(): Mapping pager frame tag=[%d] page=[%d] frame=[%d] access=[%d] P_result = [%s])\n",(int)TAG,0,FrameTable[frame].frame,USLOSS_MMU_PROT_RW,get_r(map_result));
		
		// If the state of the page is inmem or indisk dont write over!
		if(faultPage->state == UNUSED){
			pDebug(1, " <- Pager(): Page state is UNUSED, zeroing out vmRegions frame[%d]\n",frame);
			memset(vmRegion, 0, USLOSS_MmuPageSize()); //+ (FrameTable[frame].page*USLOSS_MmuPageSize()
		}else
			pDebug(1, " <- Pager(): Page state is [%d] NOT zeroing out vmRegions frame [%d]\n",faultPage->state,frame);
		
		// Update page 
		faultPage->frame = frame;
		// If page is in disk it is not in both, otherwise just in mem
		if(faultPage->state == INDISK)
			faultPage->state = INBOTH;      // See above.
		else
			faultPage->state = INMEM; 
	   
		// Temp pager mapping un-map to MMU
		map_result = USLOSS_MmuSetAccess(FrameTable[frame].frame, 0); // set frame as clean
		pDebug(1," <- Pager(): Setting frame[%d] as clean, P_result = [%s]\n",frame,get_r(map_result));
		map_result = USLOSS_MmuUnmap(TAG, 0); // unmap page
		pDebug(1," <- Pager(): Unmapping pager frame[%d], p_result = [%s]\n",frame,get_r(map_result));
		// Page fault handled, wake up faulting process
		
		// Update frame
		vmStats.new++;
		FrameTable[frame].state = F_INUSE;
		FrameTable[frame].page = (long)fm->addr / USLOSS_MmuPageSize();
		FrameTable[frame].frame = frame;
		FrameTable[frame].procPID = fm->pid;
		
		// Page fault handled, wake up faulting process
		pDebug(1," <- Pager(): Fault Handled: frame[%d] state[%s] page[%d] frams.frame[%d]\n",frame,get_r(FrameTable[frame].state),FrameTable[frame].page,FrameTable[frame].frame);
		MboxSend(fm->replyMbox,&FrameTable[frame].frame,sizeof(int));
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
   pDebug(1," <- vmDestroy(): start\n");
   CheckMode();
   vmDestroyReal();
   args->arg1 = 0; // Spec makes no mention, however, syscall returnVal is set to arg1.
   pDebug(1," <- vmDestroy(): end\n");
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
   pDebug(2," <- vmDestroyReal(): start\n");
   CheckMode();
   status = USLOSS_MmuDone();
   status=status; //Stop warnings
   
   if(VMInitialized != 1)
	   return;
   
   // Set this in advance to skip incorrect switch counts taking place from sentinel ect.
   	VMInitialized = 0;
	vmRegion = NULL;
  
  /*
    * Kill the pagers here.
    */   
	// Wake up pager all pagers and zap
	for (i = 0; i < MAXPAGERS; i++) {
		pDebug(3," <- vmDestroyReal(): zapping pager[%d] pid[%d]...\n",i,Pagers[i]);
        if (Pagers[i] == -1)
            break;
        MboxSend(fault_mbox, NULL, 0); 
		
        zap(Pagers[i]);
		join(&status);
    }

    // release fault mailboxes
    for (i = 0; i < MAXPROC; i++) {
		pDebug(3," <- vmDestroyReal(): releasing Fault Reply Mailboxes[%d]...\n",i);
        MboxRelease(FaultTable[i].replyMbox);
    }

	pDebug(3," <- vmDestroyReal(): releasing Fault Mailbox[%d]...\n",fault_mbox);
    MboxRelease(fault_mbox);
	
   /* 
    * Print vm statistics.
    */
   PrintStats();
   

   /* and so on... */
   pDebug(2," <- vmDestroyReal(): end\n");
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

void printPages(PTE *pageTable){
	USLOSS_Console("\n---------------------PAGE TABLE---------------------\n");
    USLOSS_Console(" page#  addr         state     frame  diskBlock\n");
    USLOSS_Console("----------------------------------------------------\n");
	for(i = 0;i<vmStats.pages;i++){
		USLOSS_Console("%-1s[%-2d] %-2s[%-8p] %-1s[%6s] %-1s[%-2d] %-2s[%-2d]\n","",i,"",&pageTable[i],"",get_r(pageTable[i].state),"",pageTable[i].frame,"",pageTable[i].diskBlock);
	}
	USLOSS_Console("----------------------------------------------------\n");  
}
	
	
char* get_r(int id){
	if(id<=12)
		return mmu_results[id];
	else if (id <=503)
		return page_results[id-500];
	else if (id <=601)
		return frame_results[id-600];
	else if (id <=701)
		return disk_results[id-700];
	else
		return "INVALID ID";
}