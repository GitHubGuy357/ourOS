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
#define CHILD_ZAPPED 2

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

/* PROTOTYPES */
	void cleanUp(int pid);
	void dp3();
	void ds3();
	extern void Terminate(int status);
	int terminateReal(int pid, long returnStatus);
	int start2(char *);
	extern int start3(char *);
	//int spawnReal(char *name, int (*startFunc)(char *), char *arg, int stack_size, int priority, int *pid);
	int spawnReal(char *name, int (*startFunc)(char *), char *arg, int stack_size, int priority);
	int waitReal(int *status);
	void nullsys3(systemArgs *args); // Intialize after intializeSysCalls so all syscall vecs being pointing to nullsys3;
    void intializeSysCalls();
	void spawn(systemArgs *args);
	void waitNotLinux(systemArgs *args);
	void terminate (systemArgs *args);
	void semCreate(systemArgs *args);
	int semCreateReal(int initialVal);
	void semP(systemArgs *args);
	void semPReal(int handle);
	void semV(systemArgs *args);
	void semVReal(int handle);
	void semFree(systemArgs *args);
	int semFreeReal(int semID);
	void getTimeofDay(systemArgs *args);
	void cPUTime(systemArgs *args);
	void getPID(systemArgs *args);
	int pDebug(int level, char *fmt, ...);
	void putUserMode();
	int spawnLaunch(char* func);
	
#endif /* _PHASE3_H */

