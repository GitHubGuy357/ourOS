/*
 * These are the definitions for phase 4 of the project (support level, part 2).
 */

#ifndef _PHASE4_H
#define _PHASE4_H
#include "MinQueue.h"
#include <usloss.h>
#include <usyscall.h>
#include "libuser.h"
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"
#include "providedPrototypes.h"
/*
 * Maximum line length
 */

#define MAXLINE         80
#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}
extern void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args); // Added to compile with WRONG USLOSS_Sysargs, RIGHT? cant use USLOSS
/*
 * Function prototypes for this phase.
 */

extern  int  Sleep(int seconds);
extern  int  DiskRead (void *diskBuffer, int unit, int track, int first, int sectors, int *status);
extern  int  DiskWrite(void *diskBuffer, int unit, int track, int first, int sectors, int *status);
extern  int  DiskSize (int unit, int *sector, int *track, int *disk);
extern  int  TermRead (char *buffer, int bufferSize, int unitID, int *numCharsRead);
extern  int  TermWrite(char *buffer, int bufferSize, int unitID, int *numCharsRead);
extern  int  start4(char * name);

// More prototypes
void intializeSysCalls();
int check_kernel_mode(char *procName);
void putUserMode();
int pDebug(int level, char *fmt, ...);
void nullsys4(USLOSS_Sysargs *args);

// Sys call prototypes
void sleep(USLOSS_Sysargs *args);
int sleepReal();
void diskRead(USLOSS_Sysargs *args);
int diskReadReal();
void diskWrite(USLOSS_Sysargs *args);
int diskWriteReal();
void diskSize(USLOSS_Sysargs *args);
int diskSizeReal();
void termRead(USLOSS_Sysargs *args);
int termReadReal();
void termWrite(USLOSS_Sysargs *args);
int termWriteReal();

// Structures
typedef struct procTable *procPtr;
typedef struct procTable {
    int pid;
	char name[50];
	int status;
	int PVstatus;
	long sleepAt;
	long sleepDuration;
	long sleepWakeAt;
	int semID;
	int mboxID;
} procTable; 

#define ERR_INVALID             -1
#define ERR_OK                  0

#endif /* _PHASE4_H */
