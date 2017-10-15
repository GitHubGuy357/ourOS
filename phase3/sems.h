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
