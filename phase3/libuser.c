/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <usyscall.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <libuser.h>

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}
/* GLOBALS */
	extern void (*sys_vec[MAXSYSCALLS])(systemArgs *args);
/* PROTOTYPES */
	extern int pDebug(int level, char *fmt, ...);
/*
 *  Routine:  Spawn
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *                PFV startFunc      -- pointer to the startFunction to fork
 *                void *arg     -- argument to startFunction
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                int  *pid     -- pointer to output value
 *                (output value: process id of the forked process)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Spawn(char *name, int (*startFunc)(char *), char *arg, int stack_size, int priority, int *pid){
    systemArgs sysArg;
    pDebug(3,"Spawn() - BEGIN: startFunc[%p]\n",startFunc);
    CHECKMODE;
    sysArg.number = SYS_SPAWN;
    sysArg.arg1 = startFunc;
    sysArg.arg2 = arg;
    sysArg.arg3 = (void *)(long) stack_size;
    sysArg.arg4 = (void *)(long) priority;
    sysArg.arg5 = (void *) name;
    USLOSS_Syscall((void *) &sysArg);
    *pid = (long) sysArg.arg1;
	pDebug(3,"Spawn() - END: startFunc[%p] pid[%d]\n",startFunc, *pid);
    return (long) sysArg.arg4;
} /* end of Spawn */


/*
 *  Routine:  Wait
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Wait(int *pid, int *status){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_WAIT;
    USLOSS_Syscall((void *) &sysArg);
    *pid = (long) sysArg.arg1;
    *status = (long) sysArg.arg2;
    return (long) sysArg.arg4;
    
} /* end of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate 
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
void Terminate(int status){
	pDebug(1,"Terminate(): Status[%d]\n",status);
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_TERMINATE;
    sysArg.arg1 = (void *)(long) status;
    USLOSS_Syscall((void *) &sysArg);
    return;
} /* end of Terminate */


/*
 *  Routine:  SemCreate
 *
 *  Description: Create a semaphore.
 *
 *  Arguments:
 *
 */
int SemCreate(int value, int *semaphore){
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMCREATE;
    sysArg.arg1 = (void *)(long) value;
    USLOSS_Syscall((void *) &sysArg);
    *semaphore = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of SemCreate */


/*
 *  Routine:  SemP
 *
 *  Description: "P" a semaphore.
 *
 *  Arguments:
 *
 */
int SemP(int semaphore){
		pDebug(3,"SemP(): semaphore[%d]\n",semaphore);
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMP;
    sysArg.arg1 = (void *)(long) semaphore;
    USLOSS_Syscall((void *) &sysArg);
    return (long) sysArg.arg4;
} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description: "V" a semaphore.
 *
 *  Arguments:
 *
 */
int SemV(int semaphore){
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMV;
    sysArg.arg1 = (void *)(long) semaphore;
    USLOSS_Syscall((void *) &sysArg);
    return (long) sysArg.arg4;
} /* end of SemV */


/*
 *  Routine:  SemFree
 *
 *  Description: Free a semaphore.
 *
 *  Arguments:
 *
 */
int SemFree(int semaphore){
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SEMFREE;
    sysArg.arg1 = (void *)(long) semaphore;
    USLOSS_Syscall((void *) &sysArg);
    return (long) sysArg.arg4;
} /* end of SemFree */


/*
 *  Routine:  GetTimeofDay
 *
 *  Description: This is the call entry point for getting the time of day.
 *
 *  Arguments:
 *
 */
void GetTimeofDay(int *tod)                           {
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_GETTIMEOFDAY;
    USLOSS_Syscall((void *) &sysArg);
    *tod = (long) sysArg.arg1;
    return;
} /* end of GetTimeofDay */


/*
 *  Routine:  CPUTime
 *
 *  Description: This is the call entry point for the process' CPU time.
 *
 *  Arguments:
 *
 */
void CPUTime(int *cpu)                           {
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_CPUTIME;
    USLOSS_Syscall((void *) &sysArg);
    *cpu = (long) sysArg.arg1;
    return;
} /* end of CPUTime */


/*
 *  Routine:  GetPID
 *
 *  Description: This is the call entry point for the process' PID.
 *
 *  Arguments:
 *
 */
void GetPID(int *pid)                           {
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_GETPID;
    USLOSS_Syscall((void *) &sysArg);
    *pid = (long) sysArg.arg1;
    return;
} /* end of GetPID */

/* end libuser.c */
