#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdio.h>
#include "message.h"
#include <string.h> //For memcpy

/* GLOBALS */
	procTable ProcTable[MAXPROC];
	void (*sys_vec[MAXSYSCALLS])(systemArgs *args);
	int i;
	int debugVal = 1;

/* PROTOTYPES */
	void terminateReal(int pid, long returnStatus);
	int start2(char *);
	extern int start3(char *);
	int spawnReal(char *name, int (*startFunc)(char *), char *arg, int stack_size, int priority);
	int waitReal(int *status);
	void nullsys3(systemArgs *args); // Intialize after intializeSysCalls so all syscall vecs being pointing to nullsys3;
    void intializeSysCalls();
	void spawn(systemArgs *args);
	void wait(systemArgs *args);
	void terminate (systemArgs *args);
	void semCreate(systemArgs *args);
	void semP(systemArgs *args);
	void semV(systemArgs *args);
	void semFree(systemArgs *args);
	void getTimeofDay(systemArgs *args);
	void cPUTime(systemArgs *args);
	void getPID(systemArgs *args);
	int pDebug(int level, char *fmt, ...);
	void putUserMode();
	int spawnLaunch();
	int start2(char *arg){
	pDebug(1,"start2()\n");
    int pid;
    int status;
	extern void Terminate(int status);
	
    /*
     * Check kernel mode here.
     */
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0)
		USLOSS_Halt(1);
	
    /*
     * Data structure initialization as needed...
     */
	for (i=0;i<MAXPROC;i++){
		ProcTable[i].pid = -1;
		ProcTable[i].name[0] = '\0';
		ProcTable[i].status = -1;
		ProcTable[i].parentPid = -1;
		ProcTable[i].mBox = -1;
		ProcTable[i].startFunc = NULL;// startFunction pointer to start
		ProcTable[i].arg = NULL;
		ProcTable[i].returnStatus = -1;
		ProcTable[i].childCount = 0;
		intialize_queue2(&ProcTable[i].childQuitList);
	}

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode startFunction
     * called by the test cases; spawn is the kernel-mode startFunction that
     * is called by the syscallHandler; spawnReal is the startFunction that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a startFunction named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This startFunction then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the startFunction passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
	intializeSysCalls();
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
	
	//added to compile
	return pid;
} /* start2 */



/*
 *  Routine:  spawnReal - Kenal Mode
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *                PFV startFunc      -- pointer to the startFunction to fork
 *                void *arg     -- argument to startFunction
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                (output value: process id of the forked process)
 *  Return Value: 0 means success, -1 means error occurs
 */
int spawnReal(char *name, int (*startFunc)(char *), char *arg, int stack_size, int priority){
	pDebug(1,"spawnReal()\n");
	
	// Check if conditions are within range, than call fork1 to make new process.
	int kidPid = fork1(name, spawnLaunch, arg, stack_size,priority);
    
    if (kidPid < 0) {
		// If returned pid < 0 then there was an issue getting a pid from phase1 procTable.
       USLOSS_Console("startup(): fork1 returned error, ");
       USLOSS_Console("halting...\n");
       USLOSS_Halt(1);
    }
	
	// Add new process to ProcTable
	ProcTable[kidPid%MAXPROC].pid = kidPid;
	strcpy(ProcTable[kidPid%MAXPROC].name,name);
	ProcTable[kidPid%MAXPROC].status = CHILD_ALIVE;
	ProcTable[kidPid%MAXPROC].parentPid = getpid();
	ProcTable[kidPid%MAXPROC].mBox = MboxCreate(0,100);
	ProcTable[kidPid%MAXPROC].startFunc = startFunc;// startFunction pointer to start
	ProcTable[kidPid%MAXPROC].arg = arg;
	ProcTable[kidPid%MAXPROC].returnStatus = -1;
	ProcTable[kidPid%MAXPROC].childCount = 0;
	intialize_queue2(&ProcTable[kidPid%MAXPROC].childQuitList);
	
	// Open up mBox send to wait.
	int msg;
	MboxSend(ProcTable[kidPid%MAXPROC].mBox,&msg,sizeof(void*));
	//putUserMode();
	// Return pid of new process
	return kidPid;
}	

int spawnLaunch(){
	int msg;
	int launchPID = getpid()%MAXPROC; // DO NOT REMOVE - if getpid() called after purUserMode() halts USLOSS.
	pDebug(1,"spawnLaunch()\n");
	
// block Equivilent
	int mboxresult = MboxReceive(ProcTable[getpid()%MAXPROC].mBox,&msg,sizeof(void*));
	
// Switch to usermode to run user code.
	putUserMode();
	int result = ProcTable[launchPID].startFunc(ProcTable[launchPID].arg);
    pDebug(1,"spawnLaunch() result = %d mboxresult = %d\n",result,mboxresult);
	//quit(0);
	//Terminate(getpid());
	return result; // probly wrong, had to change prototype to int spawnLaunch(), spawnLaunch is called however.
}
/*
 *  Routine:  waitReal
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int waitReal(int *status){ // Like join, makes sure kid has not quit
	pDebug(1,"waitReal()\n");
	int joinVal;
	int pid = join(&joinVal);
    *status = ProcTable[getpid()%MAXPROC].returnStatus;
    return pid;
    
} /* end of Wait */

/*
Create a user-level process. Use fork1 to create the process, then change it to usermode.
If the spawned startFunction returns, it should have the same effect as calling terminate.
Input
arg1: address of the startFunction to spawn.
arg2: parameter passed to spawned startFunction.
    sysArg.arg1 = (void *) startFunc;
    sysArg.arg2 = arg;
    sysArg.arg3 = &stack_size; // & may be wrong (void *) was here
    sysArg.arg4 = &priority; // & may be wrong (void *) was here
    sysArg.arg5 = name;
*/
void spawn(systemArgs *args){
	pDebug(1,"spawn()\n");
	int result = spawnReal(args->arg5,args->arg1,args->arg2,(long)args->arg3,(long)args->arg4);
	args->arg1 = &result;
	
	// Put OS back in usermode
	putUserMode();
}

/*
Wait for a child process to terminate.
Output
arg1: process id of the terminating child.
arg2: the termination code of the child.
*/
void wait(systemArgs *args){
	pDebug(1,"wait()\n");
}

/* Terminates the invoking process and all of its children, and synchronizes with its parent’s Wait
system call. Processes are terminated by zap’ing them.
When all user processes have terminated, your operating system should shut down. Thus, after
start3 terminates (or returns) all user processes should have terminated. Since there should
then be no runnable or blocked processes, the kernel will halt.
Input
arg1: termination code for the process.
*/
void terminate(systemArgs *args){
	pDebug(1,"terminate()\n");
	terminateReal(getpid(),(long)args->arg1);
	
}

void terminateReal(int pid,long returnStatus){
	pDebug(1,"terminateReal()\n");
	ProcTable[pid%MAXPROC].returnStatus = returnStatus;
	
	// zap 1 child at a time, then when child quits, parent get reawoke when child realizes its zapped,
	// then zap next child, and so forth
	//quit(pid); // Phase 1 must be notified process has quit.
}

/*Creates a user-level semaphore.
Input
arg1: initial semaphore value.
Output
arg1: semaphore handle to be used in subsequent semaphore system calls.
arg4: -1 if initial value is negative or no semaphores are available; 0 otherwise.
*/
void semCreate(systemArgs *args){
	pDebug(1,"semCreate()\n");
}


/*
Performs a “P” operation on a semaphore.
Input
arg1: semaphore handle.
Output
arg4: -1 if semaphore handle is invalid, 0 otherwise.
*/
void semP(systemArgs *args){
	pDebug(1,"semP()\n");
}



/*
Performs a “V” operation on a semaphore.
Input
arg1: semaphore handle.
Output
arg4: -1 if semaphore handle is invalid, 0 otherwise.
*/
void semV(systemArgs *args){
	pDebug(1,"semV()\n");
}

/*
Frees a semaphore.
Input
arg1: semaphore handle.
Output
arg4: -1 if semaphore handle is invalid, 1 if there were processes blocked on the
semaphore, 0 otherwise.
Any process waiting on a semaphore when it is freed should be terminated using the equivalent
of the Terminate system call.
*/
void semFree(systemArgs *args){
	pDebug(1,"semFree()\n");
}

/*
Returns the value of USLOSS time-of-day clock.
Output
arg1: the time of day.
*/
void getTimeofDay(systemArgs *args){
	pDebug(1,"getTimeofDay()\n");
}

/*
Returns the CPU time of the process (this is the actual CPU time used, not just the time since the
current time slice started).
Output
arg1: the CPU time used by the currently running process.
*/
void cPUTime(systemArgs *args){
	pDebug(1,"cPUTime()\n");
}

/*
Returns the process ID of the currently running process.
Output
arg1: the process ID.
*/
void getPID (systemArgs *args){
	pDebug(1,"getPID()\n");
}

 /* ------------------------------------------------------------------------
    Initialize sys_vec calls
   ------------------------------------------------------------------------*/
 void intializeSysCalls(){
    for (int i = 0; i < MAXSYSCALLS; i++) {
        sys_vec[i] = nullsys3;
    }
	sys_vec[SYS_SPAWN] = spawn;
	sys_vec[SYS_WAIT] = wait;
	sys_vec[SYS_TERMINATE] = terminate;
	sys_vec[SYS_GETTIMEOFDAY] = getTimeofDay;
	sys_vec[SYS_CPUTIME] = cPUTime;
	sys_vec[SYS_GETPID] = getPID;
	sys_vec[SYS_SEMCREATE] = semCreate;
	sys_vec[SYS_SEMP] = semP;
	sys_vec[SYS_SEMV] = semV;
	sys_vec[SYS_SEMFREE] = semFree;

 } 
  /* intializeSysCalls */
  /*
  void intializeSysCalls(){
    for (int i = 0; i < MAXSYSCALLS; i++) {
        USLOSS_Sysargs[i] = nullsys3;
    }
	USLOSS_Sysargs[SYS_SPAWN] = spawn;
	USLOSS_Sysargs[SYS_WAIT] = wait;
	USLOSS_Sysargs[SYS_TERMINATE] = terminate;
	USLOSS_Sysargs[SYS_GETTIMEOFDAY] = getTimeofDay;
	USLOSS_Sysargs[SYS_CPUTIME] = cPUTime;
	USLOSS_Sysargs[SYS_GETPID] = getPID;
	USLOSS_Sysargs[SYS_SEMCREATE] = semCreate;
	USLOSS_Sysargs[SYS_SEMP] = semP;
	USLOSS_Sysargs[SYS_SEMV] = semV;
	USLOSS_Sysargs[SYS_SEMFREE] = semFree;

 }*/ /* intializeSysCalls */

 
void nullsys3(systemArgs *args) {
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
	//Terminate instead of USLOSS_Halt(1);
	terminate(args);
} /* nullsys3 */

/* ------------------------------------------------------------------------
   pDebug - Outputs a printf-style formatted string to stdout
   ------------------------------------------------------------------------*/
int pDebug(int level, char *fmt, ...) {
	 va_list args;
	 va_start(args, fmt);
     if(debugVal >= level)
        USLOSS_VConsole(fmt,args);
	 return 1;
} /* pDebug */

void putUserMode(){
	int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
	pDebug(1,"putUserMode(): Result = %d\n",result);
}
