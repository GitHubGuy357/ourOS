/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INMEM 501
#define INDISK 502
#define INBOTH 503

/* You'll probably want more states */
#define F_UNUSED 600
#define F_INUSE 601

#define D_UNUSED 700
#define D_INUSE 701

#define LOCKED 800
#define UNLOCKED 801
/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
	int  page;       // Page offset in VM
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Frame table entry.
 */
typedef struct FTE {
    int  state;      // See above.
	int  page;
    int  ownerPID;
	int  isLocked;
} FTE;

/*
 * Per-process information.
 */

typedef struct Process *procPtr;
typedef struct Process {
	int pid;
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Add more stuff here */
	int privateMBox;
} Process;

typedef struct DiskStat *diskPtr;
typedef struct DiskStat{
	int numTracks;
	int numSects;
	int sectSize;
	int blockCount;
	int *blocks;
} DiskStat;
/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
	int frameFound;
} FaultMsg;


#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
