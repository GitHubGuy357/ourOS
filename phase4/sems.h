/* ------------------------------------------------------------------------
 sems.h

 University of Arizona
 Computer Science 452
 
 Group: James Rodgers, Ben Shinohara, and Adam Shinohara

 This code is used to implement a struct to represnt a basic 
 semaphore that utilizes mailboxes to provide mutual exclusions.

 ------------------------------------------------------------------------ */
#ifndef _SEMS_H
#define _SEMS_H
#define STATUS_PV_BLOCKED 4
#define STATUS_NOT_PV_BLOCKED 3

typedef struct semTable *semPtr;
typedef struct semTable semTable;
typedef struct semTable {
    int initialVal;
	int currentVal;
	int mBoxID;
	int mutexID;
	int processPID;
	MinQueue blockList;
} semTable;
#endif /* _SEMS_H */
