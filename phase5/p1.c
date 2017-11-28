
#include "usloss.h"
#include "vm.h"
#include "phase5.h"
#include <stdlib.h>
#define DEBUG 0
#define TAG 0

// Globals
int i;
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
// Prototypes
extern void PrintStats(void);


// p1_fork
void p1_fork(int pid){
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
	
	if(VMInitialized == 0)
		return;
	
	pDebug(1," <- p1_fork(): VM is initialized, forking pid[%d]\n",pid);
	
	// Initialize page table for process
	procPtr temp = &ProcTable5[pid];
	temp->pageTable = malloc(temp->numPages * sizeof(PTE));

	for(i = 0;i<temp->numPages;i++){
		temp->pageTable->state = UNUSED;
		temp->pageTable->page = -1;
		temp->pageTable->frame = -1;
		temp->pageTable->diskBlock = -1;  	
	}
	
} /* p1_fork */ 



// p1_switch
void p1_switch(int old, int new){
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
	

	if(VMInitialized == 0)
		return;
	
	pDebug(1," <- p1_switch(): VM is initialized, switching from old[%d] new[%d]\n",old,new);

	if(vmRegion >0){
	
		procPtr temp = &ProcTable5[old];
		PTE *pagePtr = temp->pageTable;
		
		// Un Map old process
		if (pagePtr != NULL){
			for(i=0;i< temp->numPages;i++){
				if(pagePtr->state == INMEM || pagePtr->state == INDISK){
					map_result = USLOSS_MmuUnmap(TAG, FrameTable[i].frame);
				}
			}
		}

		// Map new process
		temp = &ProcTable5[new];
		pagePtr = temp->pageTable;

		if (pagePtr != NULL){
			for(i=0;i< temp->numPages;i++){
				pagePtr = &temp->pageTable[i];
				pagePtr->page = i;
				if(pagePtr->state == INMEM || pagePtr->state == INDISK){
					map_result = USLOSS_MmuMap(TAG, pagePtr->page, temp->pageTable[i].frame, USLOSS_MMU_PROT_RW);
				}
			}
		}
	}
	if(debugVal>2)dumpProcesses();
	vmStats.switches++;
//	PrintStats();
	
} /* p1_switch */



// p1_quit
void p1_quit(int pid){
	int map_result,mapped_frame,dummy;
	procPtr temp = &ProcTable5[pid];
	
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
	
	pDebug(1," <- p1_quit(): VM is %s, and pid[%d] is quiting\n",VMInitialized == 1 ? "VMInitialized": "VM NOT initialized",pid);
	if(VMInitialized == 0 || temp->pageTable == NULL)
		return;
	if(debugVal>2)dumpProcesses();
	for(i=0;i<temp->numPages;i++){
		map_result = USLOSS_MmuGetMap(TAG, i, &mapped_frame, &dummy);
		if(map_result != USLOSS_MMU_ERR_NOMAP){
			map_result = USLOSS_MmuUnmap(TAG,temp->pageTable[i].page);
			
			// clean processes pageTable
			temp->pageTable[i].state = UNUSED;
			temp->pageTable[i].page = -1;
			temp->pageTable[i].frame = -1;
			temp->pageTable[i].diskBlock = -1;
			
			// clean frame tables frame
			FrameTable[mapped_frame].state = UNUSED;
			FrameTable[mapped_frame].frame = -1;
			FrameTable[mapped_frame].page = -1;
			FrameTable[mapped_frame].procPID = -1;
			FrameTable[mapped_frame].next = NULL;
			vmStats.freeFrames++;
		}
	}
	free(temp->pageTable); 
} /* p1_quit */