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
void dp4();
void dd4();
void dt4();
void print_status(int control);
void print_control(int control);
void intializeSysCalls();
int check_kernel_mode(char *procName);
int enableInterrupts();
void putUserMode();
int pDebug(int level, char *fmt, ...);
void nullsys4(USLOSS_Sysargs *args);
char* getOp(int op);

// Sys call prototypes
void sleep(USLOSS_Sysargs *args);
int sleepReal(long sleepDuration);
void diskRead(USLOSS_Sysargs *args);
int diskReadReal(void *dbuff, int track, int first, int sectors, int unit);
void diskWrite(USLOSS_Sysargs *args);
int diskWriteReal(void *dbuff, int track, int first, int sectors, int unit);
void diskSize(USLOSS_Sysargs *args);
int diskSizeReal(int unit);
void termRead(USLOSS_Sysargs *args);
int termReadReal(int unit, int size, char *buff);
void termWrite(USLOSS_Sysargs *args);
int termWriteReal(int unit, int size, char *buff);

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
	int mBoxID;
	int disk_sector_size;
	int disk_track_size; 
	int disk_size;
	int diskOp;
	void *dbuff;
	int track;
	int first;
	int sectors;
	int unit;
	int t_Op;
	void *t_buff;
	int t_buff_size;
    int t_unit;
	int t_controlStatus;
} procTable; 

typedef struct diskTable *diskTablePtr;
typedef struct diskTable {
    int pid;
	int currentOp;
	int currentTrack;
	int currentSector;
	int disk_sector_size;
	int disk_track_size;
	int disk_size;
	int semID;
	int mBoxID;
	MinQueue DriveQueueR;
	MinQueue DriveQueueL;
	int drive_seek_dir; // 0 = left, 1 = right
} diskTable; 

typedef struct termTable *termTablePtr;
typedef struct termTable {
    int pid;
	char type[20];
	int currentOp;
	int semID;
	int t_write_semID;
	int mBoxID;
	int t_controlStatus;
	char receiveChar;
	int lineNumber;
	char t_buffer_of_lines[10][MAXLINE];
	MinQueue requestQueue;
} termTable; 

#define ERR_INVALID         -1
#define ERR_OK              0
#define USLOSS_DISK_SIZE	4
#define USLOSS_TERM_WRITE   1
#define USLOSS_TERM_READ    2
#define MAX_LINES     10 
#define STATUS_QUIT         0
#define STATUS_RUNNING      1
#endif /* _PHASE4_H */
