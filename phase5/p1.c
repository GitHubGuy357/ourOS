/* ------------------------------------------------------------------------
 p1.c

 University of Arizona
 Computer Science 452
 
 Group: James Rodgers, Ben Shinohara, and Adam Shinohara

 This code is used to assist in creating a model implementing a virtual memory (VM) system that supports 
 demand paging. The USLOSS MMU is used to configure a region of virtual memory whose 
 contents are process-specific. 

 ------------------------------------------------------------------------ */


#include "usloss.h"
#include "vm.h"
#include "phase5.h"
#include <stdlib.h>
#define DEBUG 0
#define TAG 0

// Globals
int i,framePtr,frameProtPtr;
int map_result;
extern int debugflag;
extern int VMInitialized;
extern Process ProcTable5[];
extern FTE *FrameTable;
extern VmStats vmStats;
extern void *vmRegion;
extern int pDebug(int level, char *fmt, ...);
extern int debugVal;
extern void dp5();
extern char* get_r(int id);
extern char mmu_results[12][40];
extern char page_results[4][10];
extern char frame_results[2][10];
extern char disk_results[2][10];
extern DiskStat Disk;

// Prototypes
extern void PrintStats(void);
extern void printPages(PTE *pageTable);


// p1_fork
/*
 *----------------------------------------------------------------------
 *
 * p1_fork --
 *
 * Initializes the page table when a process is forked. 
 *
 * Results:
 *      Page table is initialized
 *
 * Inputs:
 *      Process ID.
 *
 *----------------------------------------------------------------------
 */
void p1_fork(int pid){
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
	
	if(VMInitialized == 0)
		return;
	
	pDebug(2," <- p1_fork(): VM is initialized, forking pid[%d]\n",pid);
	
	// Initialize page table for process
	procPtr temp = &ProcTable5[pid];
	temp->pageTable = calloc(temp->numPages,sizeof(PTE));

	for(i = 0;i<temp->numPages;i++){
		pDebug(3," <- p1_fork(): Creating page[%d] for pid[%d]\n",i,pid);
		temp->pageTable[i].state = UNUSED;
		temp->pageTable[i].page = -1;
		temp->pageTable[i].frame = -1;
		temp->pageTable[i].diskBlock = -1;
	}
	 if(debugVal>2)printPages(temp->pageTable);
} /* p1_fork */ 

/*
 *----------------------------------------------------------------------
 *
 * p1_switch --
 *
 * During a context switch, this function unloads all of the mappings from
 * the old process and loads all of the valid mappings for the new process. 
 *
 * Results:
 *      The mapping for the new process is loaded
 *
 * Inputs:
 *      Old : the old process.
 *		New : the new process.
 *----------------------------------------------------------------------
 */
void p1_switch(int old, int new){
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
	
	//have to make sure the vm has been initialized
	if(VMInitialized == 0)
		return;
	
	pDebug(1," <- p1_switch(): VM is initialized, switching from old[%d] new[%d]\n",old,new);

	//Create a temp pointer to loop through the old process to unmap it
	if(VMInitialized == 1){
		procPtr temp = &ProcTable5[old];
		PTE *pagePtr = temp->pageTable;
		
		// Un Map old process
		if (pagePtr != NULL){
			for(i=0;i< temp->numPages;i++){
				map_result = USLOSS_MmuGetMap(TAG, i, &framePtr, &frameProtPtr); 
				if(map_result != USLOSS_MMU_ERR_NOMAP){ 
				//if(pagePtr->state == INMEM){  // Check if page is mapped, if so unmap it.
					map_result = USLOSS_MmuUnmap(TAG, i); //temp->pageTable[i].page
					pDebug(1," <- p1_switch(): Unmapping page=[%d] from frame[%d] by old pid[%d]... S_result = [%s]\n",temp->pageTable[i].page,temp->pageTable[i].frame,old,get_r(map_result));
				}
				pagePtr++;
			}
		}
		
		//Set the temp variable to the new process that needs to be mapped
		temp = &ProcTable5[new];
		pagePtr = temp->pageTable;
		
		// Map new process
		if (pagePtr != NULL){
			if(debugVal>1)printPages(temp->pageTable);
			for(i=0;i< temp->numPages;i++){
				pagePtr = &temp->pageTable[i];
				if(pagePtr->state == INMEM || pagePtr->state == INBOTH){ // Check if page is mapped, if so map it. ??? //TODO
					pagePtr->page = i;
					map_result = USLOSS_MmuMap(TAG, i, temp->pageTable[i].frame, USLOSS_MMU_PROT_RW); //temp->pageTable[i].page
					pDebug(1," <- p1_switch(): Mapping page=[%d] status[%s] to frame[%d] by new pid[%d]... S_result = [%s]\n",temp->pageTable[i].page,get_r(temp->pageTable[i].state),temp->pageTable[i].frame,new,get_r(map_result));
				}
				pagePtr++;
			}
		}
		if(debugVal>2)dumpProcesses();
		vmStats.switches++;
	}
} /* p1_switch */

/*
 *----------------------------------------------------------------------
 *
 * p1_quit --
 *
 * When the process quits, clean up the page table that was created,
 * this includes freeing the memory allocated for the table 
 *
 * Results:
 *     The page tble is cleaned up
 *
 * Inputs:
 *      Process ID.
 *
 *----------------------------------------------------------------------
 */
void p1_quit(int pid){
	int map_result,mapped_frame,dummy;
	procPtr temp = &ProcTable5[pid];
	
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
	
	pDebug(2," <- p1_quit(): VM is %s, and pid[%d] is quiting\n",VMInitialized == 1 ? "VMInitialized": "VM NOT initialized",pid);
	
	//Check to make sure that the vm was initialized and the table exists
	if(VMInitialized == 0 || temp->pageTable == NULL)
		return;
	if(debugVal>1)dumpProcesses();
	
	//Loop through the page table of the process to clean it up
	for(i=0;i<temp->numPages;i++){
		map_result = USLOSS_MmuGetMap(TAG, i, &mapped_frame, &dummy);
		if(map_result != USLOSS_MMU_ERR_NOMAP){
			pDebug(1," <- p1_quit(): Unmapping page=[%d] from frame[%d]\n",i,mapped_frame);
			map_result = USLOSS_MmuUnmap(TAG,i); //temp->pageTable[i].page
			
			// clean processes pageTable
			temp->pageTable[i].state = UNUSED;
			temp->pageTable[i].page = -1;
			temp->pageTable[i].frame = -1;
			
			// clean frame tables frame
			FrameTable[mapped_frame].state = F_UNUSED;
			FrameTable[mapped_frame].page = -1;
			FrameTable[mapped_frame].ownerPID = -1;
			FrameTable[mapped_frame].isLocked = UNLOCKED;
			
			// If 3 process runs and use 2 frames, when they quit, this will be 3 free frames, and this corrects that
			if(vmStats.freeFrames < vmStats.frames)
			vmStats.freeFrames++;
		}
		if(temp->pageTable[i].diskBlock > -1 && Disk.blocks[temp->pageTable[i].diskBlock] == D_INUSE){
			pDebug(1," <- p1_quit(): Clean diskblock[%d] with status of[%s]\n",temp->pageTable[i].diskBlock,get_r(Disk.blocks[temp->pageTable[i].diskBlock]));
			Disk.blocks[temp->pageTable[i].diskBlock] = D_UNUSED;	
			vmStats.freeDiskBlocks++;
		}
	}
	pDebug(1," <- p1_quit(): Process cleaned, freeing pagetable mem[%p]\n",temp->pageTable);
	
	//free the memory allocated for the table
	free(temp->pageTable); 
} /* p1_quit */