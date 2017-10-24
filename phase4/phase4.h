/*
 * These are the definitions for phase 4 of the project (support level, part 2).
 */

#ifndef _PHASE4_H
#define _PHASE4_H
#include "MinQueue.h"
/*
 * Maximum line length
 */

#define MAXLINE         80

/*
 * Function prototypes for this phase.
 */

extern  int  Sleep(int seconds);
extern  int  DiskRead (void *diskBuffer, int unit, int track, int first, int sectors, int *status);
extern  int  DiskWrite(void *diskBuffer, int unit, int track, int first, int sectors, int *status);
extern  int  DiskSize (int unit, int *sector, int *track, int *disk);
extern  int  TermRead (char *buffer, int bufferSize, int unitID, int *numCharsRead);
extern  int  TermWrite(char *buffer, int bufferSize, int unitID, int *numCharsRead);
extern  int  start4(char *);

// More prototypes
int check_kernel_mode(char *procName);

typedef struct procTable *procPtr;
typedef struct procTable {
    int pid;
	int parentPID;
	int priority;
	char name[50];
	int status;
	int PVstatus;
	int childCount;
	MinQueue childList;
	int mBoxID;
    int (*startFunc)(char *);// startFunction pointer to start
    char *arg;
    long returnStatus;
} procTable; 

#define ERR_INVALID             -1
#define ERR_OK                  0

#endif /* _PHASE4_H */
