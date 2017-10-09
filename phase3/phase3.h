/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS         200
#define CHILD_ALIVE 1
#define CHILD_DEAD 0

typedef struct procTable {
    int pid;
	int parentPid;
	char name[MAXNAME];
	int status;
	int childCount;
	MinQueue childQuitList;
	int mailBox;
    int (*func)(char *);// Function pointer to start
    char *arg;
    long returnStatus;
} procTable;
#endif /* _PHASE3_H */

