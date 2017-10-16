/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H
#include <time.h>
#include "MinQueue.h"

#define MAXSEMS         200
#define CHILD_ALIVE 0
#define CHILD_DEAD 1

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
#endif /* _PHASE3_H */

