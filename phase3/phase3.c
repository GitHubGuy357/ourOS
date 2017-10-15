#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdio.h>
#include "message.h"
#include <string.h> //For memcpy
#include "sems.h"

/* GLOBALS */
	procTable ProcTable[MAXPROC];
	semTable SemTable[MAXSEMS];
	void (*systemCallVec[MAXSYSCALLS])(systemArgs *args);
	int i;
	int debugVal = 0; // Level of debug info: 1)Most 2)Mediulm 3)Least 0)off

/* PROTOTYPES */
	void dp3();
	void ds3();
	extern void Terminate(int status);
	void terminateReal(int pid, long returnStatus);
	int start2(char *);
	extern int start3(char *);
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
	int start2(char *arg){
	pDebug(2,"start2()\n");
    int pid;
    int status;
	
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
		ProcTable[i].parentPID = -1;
		ProcTable[i].priority = -1;
		ProcTable[i].name[0] = '\0';
		ProcTable[i].status = -1;
		ProcTable[i].PVstatus = -1;
		ProcTable[i].mBoxID = -1;
		ProcTable[i].startFunc = NULL;// startFunction pointer to start
		ProcTable[i].arg = NULL;
		ProcTable[i].returnStatus = -1;
		ProcTable[i].childCount = 0;
		intialize_queue2(&ProcTable[i].childList);
	}

	for (i=0;i<MAXSEMS;i++){
		SemTable[i].initialVal = -1;
		SemTable[i].currentVal = -1;
		SemTable[i].mBoxID = -1;
		SemTable[i].mutexID = -1;
		SemTable[i].processPID = -1;
		intialize_queue2(&SemTable[i].blockList);
	}
	
    /*
     * Create first user-level process and waitNotLinux for it to finish.
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

    /* Call the waitReal version of your waitNotLinux code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
	
	//added to compile
	return pid;
} /* start2 */


int spawnLaunch(char * func){ // Analougous to Phase 1 launch()
	int msg;
	int mBoxresult;
	int launchPID = getpid()%MAXPROC; // DO NOT REMOVE - if getpid() called after purUserMode() halts USLOSS.
	pDebug(2,"spawnLaunch() - START: startFunc = [%p]\n",func);
	
// unblock Equivilent
	if(ProcTable[launchPID].mBoxID == -1){
		ProcTable[launchPID].mBoxID = MboxCreate(0,100);
		pDebug(2,"spawnLaunch() - MailBox is NULL\n");
	}else{
		pDebug(2,"spawnLaunch() - MailBox is %d\n",ProcTable[getpid()%MAXPROC].mBoxID);
	}
	mBoxresult = MboxReceive(ProcTable[getpid()%MAXPROC].mBoxID,&msg,sizeof(void*));
	
// Switch to usermode to run user code.
	putUserMode();
	
// launch process
	pDebug(1,"\nspawnLaunch(): Launching_PID = [%d] launchName = [%s] launchFunc = [%p]\n",launchPID,ProcTable[launchPID].name,ProcTable[launchPID].startFunc);
	int result = ProcTable[launchPID].startFunc(ProcTable[launchPID].arg);
    pDebug(2,"spawnLaunch() - END: result = %d mBoxIDresult = %d\n",result,mBoxresult);
	//quit(result);
	Terminate(launchPID);
	return result; // probly wrong, had to change prototype to int spawnLaunch(), spawnLaunch is called however.
}

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
	pDebug(2,"spawn(): BEGIN\n");
	pDebug(2,"spawn(): getpid[%d] startFunc[%p] args[%s] stack_size[%d] priority[%d] name[%s]\n",getpid(),args->arg1,args->arg2,(long)args->arg3,(long)args->arg4,args->arg5);
	int pid = spawnReal(args->arg5,args->arg1,args->arg2,(long)args->arg3,(long)args->arg4);
	pDebug(2,"spawn(): pid[%d]\n",pid);
	args->arg1 = (void*)(long)pid;
	
	// Put OS back in usermode
	putUserMode();
	pDebug(2,"spawn(): END\n");
}

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
// TODO: Check if conditions are within range, than call fork1 to make new process.
	pDebug(2,"spawnReal() - START Before fork1():\n");
	int kidPid = fork1(name, spawnLaunch, arg, stack_size,priority);
    pDebug(2,"spawnReal() - After fork1(): CurrentPID[%d] kidpid[%d] startFunc[%p] args[%s] stack_size[%d] priority[%d] name[%s]\n",getpid(),kidPid,startFunc,arg,stack_size,priority,name);
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
	ProcTable[kidPid%MAXPROC].parentPID = getpid();
	ProcTable[kidPid%MAXPROC].priority = priority;
	if(ProcTable[kidPid%MAXPROC].mBoxID == -1){
		pDebug(2,"spawnReal() - MailBox is NULL\n");
		ProcTable[kidPid%MAXPROC].mBoxID = MboxCreate(0,100);
	}else{
		pDebug(2,"spawnReal() - MailBox is %d\n",ProcTable[kidPid%MAXPROC].mBoxID);
	}
	ProcTable[kidPid%MAXPROC].startFunc = startFunc;// startFunction pointer to start
	ProcTable[kidPid%MAXPROC].arg = arg;
	ProcTable[kidPid%MAXPROC].returnStatus = -1;
	intialize_queue2(&ProcTable[kidPid%MAXPROC].childList);
	
// Add to child count like fork1()
	ProcTable[getpid()%MAXPROC].childCount++;
	push(&ProcTable[getpid()%MAXPROC].childList,(long long)time(NULL),&ProcTable[kidPid%MAXPROC]);
	
// Print Process Table
	// dp3();
	
// Open up mBoxsend to waitNotLinux.
	int msg;
	MboxSend(ProcTable[kidPid%MAXPROC].mBoxID,&msg,sizeof(void*));
	
// Put back into user mode
	// putUserMode();
	
// Return pid of new process
	pDebug(2,"spawnReal() - END\n");
	return kidPid;
}	

/*
Wait for a child process to terminate.
Output
arg1: process id of the terminating child.
arg2: the termination code of the child.
*/
void waitNotLinux(systemArgs *args){
	pDebug(1,"waitNotLinux()\n");
	int status;
	int pid = waitReal(&status);
	args->arg1 = (void*)(long)pid;
	args->arg2 = (void*)(long)status;
	//putUserMode();
}

/*
 *  Routine:  waitReal
 *
 *  Description: This is the call entry to waitNotLinux for a child completion
 *
 *  Arguments:    int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int waitReal(int *status){ // Like join, makes sure kid has not quit
	pDebug(1,"waitReal(): Begin\n");
	int joinVal;
	int pid = join(&joinVal);
    *status = ProcTable[pid%MAXPROC].returnStatus;
	pDebug(2,"waitReal(): End - CurrentPID[%d] joinPID[%d] joinVal[%d]\n",getpid(),pid,joinVal);
	//putUserMode();
    return pid;
} /* end of Wait */

/* Terminates the invoking process and all of its children, and synchronizes with its parent’s Wait
system call. Processes are terminated by zap’ing them.
When all user processes have terminated, your operating system should shut down. Thus, after
start3 terminates (or returns) all user processes should have terminated. Since there should
then be no runnable or blocked processes, the kernel will halt.
Input
arg1: termination code for the process.
*/
void terminate(systemArgs *args){
	pDebug(2,"terminate()\n");
	terminateReal(getpid(),(long)args->arg1);
}

void terminateReal(int pid,long returnStatus){
	if(debugVal>0){
		dp3();
		dumpProcesses();
	}
	if(ProcTable[pid%MAXPROC].childList.count >0){
		pDebug(1,"terminateReal(): pid[%d] has [%d] children:\n",pid,ProcTable[pid%MAXPROC].childCount);
		if(debugVal>0)
			printQ(ProcTable[pid%MAXPROC].childList);
		if(ProcTable[pid%MAXPROC].childList.count>0){
			procPtr tempChild = pop(&ProcTable[pid%MAXPROC].childList);
			// zap 1 child at a time, then when child quits, parent get reawoke when child realizes its zapped,
			// then zap next child, and so forth
			if(tempChild->status == CHILD_ALIVE){
				//pDebug(0,"ZAPPING\n");
				zap(tempChild->pid);
			}
			pDebug(1,"terminateReal(): after zap\n");
		}
	}else{
		pDebug(1,"terminateReal(): pid[%d] has 0 children\n",pid);
		ProcTable[pid%MAXPROC].returnStatus = returnStatus;
		ProcTable[pid%MAXPROC].status = CHILD_DEAD;
		remove_data(&ProcTable[ProcTable[pid%MAXPROC].parentPID%MAXPROC].childList,pid%MAXPROC);
		if(debugVal>0)
				printQ(ProcTable[pid%MAXPROC].childList);
		pDebug(1,"terminateReal(): END - pid[%d] returnStatus[%d]\n",pid,returnStatus);

		quit(returnStatus); // Phase 1 must be notified process has quit.
	}
}

/*Creates a user-level semaphore.p ProcTable[pid%50].childList.count
Input
arg1: initial semaphore value.
Output
arg1: semaphore handle to be used in subsequent semaphore system calls.
arg4: -1 if initial value is negative or no semaphores are available; 0 otherwise.
*/
void semCreate(systemArgs *args){
	pDebug(2,"semCreate()\n");
    int semVal = semCreateReal((long)args->arg1);
	if(semVal == -1 || (long)args->arg1 <0)
		args->arg4 = (void*)(long)-1;
	else{
		args->arg1 = (void *)(long)semVal;
		args->arg4 = (void*)(long)0;
	}
	putUserMode();
//TODO: If zapped;
}

int semCreateReal(int initialVal){
	int returnVal = -1;
	for(i=0;i<MAXSEMS;i++){
		if(SemTable[i].mBoxID == -1){
			SemTable[i].initialVal = initialVal;
			SemTable[i].currentVal = 0;
			SemTable[i].processPID = getpid()%MAXPROC;
			SemTable[i].mBoxID = MboxCreate(initialVal,0);
			SemTable[i].mutexID = MboxCreate(0,0);
			returnVal = i;
			break;
		}
	}
	putUserMode();
	return returnVal;
}

/*
Performs a “P” operation on a semaphore.
Input
arg1: semaphore handle.
Output
arg4: -1 if semaphore handle is invalid, 0 otherwise.
*/
void semP(systemArgs *args){
	pDebug(2,"semP()\n");
	if(SemTable[(long)args->arg1].mBoxID != -1){
		semPReal((long)args->arg1);
		args->arg4 = (void*)(long)0;
	}else
		args->arg4 = (void*)(long)-1;
}

void semPReal(int semID){
	int recieveResult;
	
	//get mutex
//	MboxSend(SemTable[semID].mutexID,&recieveResult,0);
	
	pDebug(2,"semPReal(): CurrentPID = [%d]\n",getpid()%MAXPROC);
	SemTable[semID].processPID = getpid()%MAXPROC;
	if(SemTable[semID].currentVal >=0){
		// If we are going to block on this send, update process status to blocked for semFree()
		if(SemTable[semID].currentVal == SemTable[semID].initialVal){
			push(&SemTable[semID].blockList,(long long)time(NULL),&ProcTable[SemTable[semID].processPID]);
			ProcTable[SemTable[semID].processPID].PVstatus = STATUS_PV_BLOCKED;
		}else
			SemTable[semID].currentVal--;
		if(debugVal>0){
			ds3();
			dp3();
		}
			MboxSend(SemTable[semID].mBoxID,&recieveResult,0);
			if(SemTable[semID].mBoxID == -1){
				terminateReal(getpid()%MAXPROC,-111);
				pDebug(1,"semPReal(): After MboxSend(SemTable[semID].mBoxID,&recieveResult,0);\n\n\n\n");
			}
		}
		
	putUserMode();
	
	//release mutex
	//MboxReceive(SemTable[semID].mutexID,&recieveResult,0);
}

/*
Performs a “V” operation on a semaphore.
Input
arg1: semaphore handle.
Output
arg4: -1 if semaphore handle is invalid, 0 otherwise.
*/
void semV(systemArgs *args){
	pDebug(2,"semV()\n");
	if(SemTable[(long)args->arg1].mBoxID != -1){
		semVReal((long)args->arg1);
		args->arg4 = (void*)(long)0;
	}else
		args->arg4 = (void*)(long)-1;
}

void semVReal(int semID){
	int sendResult;
		
	//get mutex
//	MboxSend(SemTable[semID].mutexID,&sendResult,0);
	
	pDebug(2,"semVReal(): CurrentPID = [%d]\n",getpid()%MAXPROC);
	SemTable[semID].processPID = getpid()%MAXPROC;
	if(SemTable[semID].currentVal <= SemTable[semID].initialVal){
		if (SemTable[semID].blockList.count>0){
			procPtr tempProc = pop(&SemTable[semID].blockList);
			ProcTable[tempProc->pid].PVstatus = STATUS_NOT_PV_BLOCKED;
		}
	}else		
		SemTable[semID].currentVal++;
	if(debugVal>0){
		ds3();
		dp3();
	}
	MboxReceive(SemTable[semID].mBoxID,&sendResult,0);
	pDebug(1,"semVReal(): After MboxReceive(SemTable[semID].mBoxID,&sendResult,0);");
	putUserMode();
	
	//release mutex
//	MboxReceive(SemTable[semID].mutexID,&sendResult,0);
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
	pDebug(2,"semFree()\n");
	if(SemTable[(long)args->arg1].mBoxID != -1){
		if(semFreeReal((long)args->arg1)){
			args->arg4 = (void*)(long)0;
		}else{
			args->arg4 = (void*)(long)1;
		}			
	}else{
		args->arg4 = (void*)(long)-1;
	}
	putUserMode();
}

int semFreeReal(int semID){
	int returnVal = 0;
	pDebug(2,"semFreeReal()\n");
	if(SemTable[semID].blockList.count==0)
		returnVal = 0;
	else{ // TODO : TERMINATE REAL!!!!!
	//	while(SemTable[semID].blockList.count >0){	
	//		terminateReal(pop(&SemTable[semID].blockList)->pid,-111);
			//zap(pop(&SemTable[semID].blockList)->pid);
	//	}
	}
	returnVal = 1;
	
	int mBoxID = SemTable[semID].mBoxID;
	SemTable[semID].mBoxID = -1; // use this val in resume of blocked P to termin children if freeing semaphore
	SemTable[semID].initialVal = -1;
	SemTable[semID].currentVal = -1;
	SemTable[semID].processPID = -1;
	SemTable[semID].processPID = -1;
	intialize_queue2(&SemTable[semID].blockList);
	MboxRelease(mBoxID);



	putUserMode();
	return returnVal;
}
/*
Returns the value of USLOSS time-of-day clock.
Output
arg1: the time of day.
*/
void getTimeofDay(systemArgs *args){
	pDebug(2,"getTimeofDay()\n");
	putUserMode();
}

/*
Returns the CPU time of the process (this is the actual CPU time used, not just the time since the
current time slice started).
Output
arg1: the CPU time used by the currently running process.
*/
void cPUTime(systemArgs *args){
	pDebug(2,"cPUTime()\n");
	putUserMode();
}

/*
Returns the process ID of the currently running process.
Output
arg1: the process ID.
*/
void getPID (systemArgs *args){
	pDebug(2,"getPID()\n");
	args->arg1 = (void*)(long)getpid(); //TODO: This should not work because we must not be in user mode as we just did a system call...UNLESS USLOSS magic is performed...like always...
	putUserMode();
}

 /* ------------------------------------------------------------------------
    Initialize systemCallVec calls
   ------------------------------------------------------------------------*/
 void intializeSysCalls(){
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i] = nullsys3;
    }
	systemCallVec[SYS_SPAWN] = spawn;
	systemCallVec[SYS_WAIT] = waitNotLinux;
	systemCallVec[SYS_TERMINATE] = terminate;
	systemCallVec[SYS_GETTIMEOFDAY] = getTimeofDay;
	systemCallVec[SYS_CPUTIME] = cPUTime;
	systemCallVec[SYS_GETPID] = getPID;
	systemCallVec[SYS_SEMCREATE] = semCreate;
	systemCallVec[SYS_SEMP] = semP;
	systemCallVec[SYS_SEMV] = semV;
	systemCallVec[SYS_SEMFREE] = semFree;

 } 
  /* intializeSysCalls */
  /*
  void intializeSysCalls(){
    for (int i = 0; i < MAXSYSCALLS; i++) {
        USLOSS_Sysargs[i] = nullsys3;
    }
	USLOSS_Sysargs[SYS_SPAWN] = spawn;
	USLOSS_Sysargs[SYS_WAIT] = waitNotLinux;
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
    USLOSS_Console("nullsys3(): Invalid syscall %d. Halting...\n", args->number);
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
	pDebug(2,"putUserMode(): Result = %d\n",result);
}

void dp3(){
    USLOSS_Console("\n------------------------PROCESS TABLE-----------------------\n");
    USLOSS_Console(" PID  ParentPID Priority  Status PVstatus #kids  Name        mBoxID\n");
    USLOSS_Console("------------------------------------------------------------\n");
		for( i= 0; i< MAXPROC;i++){
			if(ProcTable[i].pid != -1){ // Need to make legit determination for printing process
				USLOSS_Console("%-1s[%-2d] %s[%-2d] %-5s[%d] %-6s[%-2d] %-6s[%-2d] %-3s[%-2d] %-2s[%-7s] %-2s[%-2d]\n"
						,"",ProcTable[i].pid,"", ProcTable[i].parentPID,"",ProcTable[i].priority,"",
						ProcTable[i].status,"",ProcTable[i].PVstatus,"",ProcTable[i].childCount,"",ProcTable[i].name,"",ProcTable[i].mBoxID);
			}	  
		}
    USLOSS_Console("------------------------------------------------------------\n");    
}

void ds3(){
    USLOSS_Console("\n------------------------SEM TABLE-----------------------\n");
    USLOSS_Console(" initialVal  currentVal mBoxID  processPID\n");
    USLOSS_Console("------------------------------------------------------------\n");
		for( i= 0; i< MAXPROC;i++){
			if(SemTable[i].initialVal != -1){ // Need to make legit determination for printing process
				USLOSS_Console("%-1s[%-2d] %-7s[%-2d] %-6s[%d] %-5s[%-2d]\n"
						,"",SemTable[i].initialVal,"", SemTable[i].currentVal,"",SemTable[i].mBoxID,"",
						SemTable[i].processPID);
			}	  
		}
    USLOSS_Console("------------------------------------------------------------\n");    
}