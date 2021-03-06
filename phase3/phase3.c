/* ------------------------------------------------------------------------
 phase3.c

 University of Arizona
 Computer Science 452
 
 Group: James Rodgers, Ben Shinohara, and Adam Shinohara

 This code is used to simulate using semaphores to execute critical sections of code on
 an operating system. This also requires that several interrupts be enabled,
 and implemented. This code spawns user processes and also terminates them. 
 Mailboxes to send and receive are used to implement semaphores to provide 
 mutual exclusion.

 ------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include <libuser.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdio.h>
#include <string.h> //For memcpy
#include "sems.h"

/* GLOBALS */
	procTable ProcTable[MAXPROC];
	semTable SemTable[MAXSEMS];
	void (*systemCallVec[MAXSYSCALLS])(systemArgs *args);
	int i;
	int debugVal = 0; // Level of debug info: 1)Most 2)Mediulm 3)Least 0)off

/*****************************************************************************
 *  Routine:  start2
 *
 *	Begin by initializing both the process table and semaphore tables
 *	Syscalls will also be initialized in this function. Return pid of the 
 *	first process that is created.
 * 
**********************************************************************************************/

int start2(char *arg){
	pDebug(3," <- start2()\n");
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
	 
	 //Initialize the process table here
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

	//Initialize the semaphore table here
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

/*****************************************************************************
 *  Routine:  spawn
 *
 *  Create a user-level process. Use fork1 to create the process, then change it to usermode.
 *  If the spawned startFunction returns, it should have the same effect as calling terminate.
 *  Input
 * 		arg1: address of the startFunction to spawn.
 * 		arg2: parameter passed to spawned startFunction.
 *		arg3: stack size (in bytes).
 *		arg4: priority.
 *		arg5: character string containing process’s name.
 *	Output
 *		arg1: the PID of the newly created process; -1 if a process could not be created.
 * 		arg4: -1 if illegal values are given as input; 0 otherwise.
**********************************************************************************************/
void spawn(systemArgs *args){
	pDebug(3," <- spawn(): BEGIN\n");
	pDebug(3," <- spawn(): getpid[%d] startFunc[%p] args[%s] stack_size[%d] priority[%d] name[%s]\n",getpid(),args->arg1,args->arg2,(long)args->arg3,(long)args->arg4,args->arg5);
	int pid = spawnReal(args->arg5,args->arg1,args->arg2,(long)args->arg3,(long)args->arg4);
	pDebug(3," <- spawn(): pid[%d]\n",pid);
	
	// Check if conditions are within range, than call fork1 to make new process.
	if(pid > -1){
		args->arg1 = (void*)(long)pid;
		if((long)args->arg4 > -1) 
			args->arg4 = (void*)(long)0;
		else
			args->arg4 = (void*)(long)-1;
	}else
		args->arg1 = (void*)(long)-1;
	

	pDebug(3," <- spawn(): END\n");
}

/*****************************************************************************
 *  Routine:  spawnReal - Kenal Mode
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *                PFV startFunc      -- pointer to the startFunction to fork
 *                void *arg     -- argument to startFunction
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                (output value: process id of the forked process)
 *  Return Value: 0 means success, -1 means error occurs
 ******************************************************************************/
int spawnReal(char *name, int (*startFunc)(char *), char *arg, int stack_size, int priority){
	pDebug(3," <- spawnReal() -> START Before fork1():\n" );
	
	//fork to create the new process
	int kidPid = fork1(name, spawnLaunch, arg, stack_size,priority);
    pDebug(2," <- spawnReal() -> After fork1(): cPID[%d] kPID[%d] kName[%s] startFunc[%p] args[%s] stack_size[%d] priority[%d]\n",getpid(),kidPid,name,startFunc,arg,stack_size,priority);

	// Check if conditions are within range, than call fork1 to make new process.
	// If returned pid < 0 then there was an issue getting a pid from phase1 procTable.
	// Values could have been out of range, missing function names, stack_size, or priority
	// out of bounds.
    if (kidPid < 0) {
		return -1;
    }
	
	// Add new process to ProcTable
	ProcTable[kidPid%MAXPROC].pid = kidPid;
	strcpy(ProcTable[kidPid%MAXPROC].name,name);
	ProcTable[kidPid%MAXPROC].status = CHILD_ALIVE;
	ProcTable[kidPid%MAXPROC].parentPID = getpid();
	ProcTable[kidPid%MAXPROC].priority = priority;
	
	//check and see if a mailbox has been created for the process yet,
	//if no mailbox has been created, initialize one
	if(ProcTable[kidPid%MAXPROC].mBoxID == -1){
		ProcTable[kidPid%MAXPROC].mBoxID = MboxCreate(0,100);
		pDebug(3," <- spawnReal() - Creating MboxID = [%d]\n",ProcTable[kidPid%MAXPROC].mBoxID);
	}else{
		pDebug(3," <- spawnReal() - MboxID = [%d]\n",ProcTable[kidPid%MAXPROC].mBoxID);
	}
	ProcTable[kidPid%MAXPROC].startFunc = startFunc;// startFunction pointer to start
	ProcTable[kidPid%MAXPROC].arg = arg;
	ProcTable[kidPid%MAXPROC].returnStatus = -1;
	intialize_queue2(&ProcTable[kidPid%MAXPROC].childList);
	
	// Add to child count like fork1()
	ProcTable[getpid()%MAXPROC].childCount++;
	
	//Add the process to the queue
	push(&ProcTable[getpid()%MAXPROC].childList,(long long)time(NULL),&ProcTable[kidPid%MAXPROC]);
	
	// Unblocks child if was blocked.
	pDebug(2," <- spawnReal() - unblocking child = [%d] startFunc=[%p] mboxID=[%d]\n",ProcTable[kidPid%MAXPROC].pid,ProcTable[kidPid%MAXPROC].startFunc,ProcTable[kidPid%MAXPROC].mBoxID);
	
	// A conditional send is used to unblock a child, if it was blocked.
	// Conditional send is used because 2 possibilities occur
	//  1. The parent is higher priority than the child and spawnreal will run to completeion
	//  and there will be nothing to unblock
	//  2. The parent is lower priority than the child and spawnreal will unblock the child
	//  from where it blocked in spawn launch
	int msg;
	MboxCondSend(ProcTable[kidPid%MAXPROC].mBoxID,&msg,sizeof(void*));

	// Statements used for debugging
	if(debugVal>2){
		dp3();
		dumpProcesses();
	}
	if(ProcTable[kidPid%MAXPROC].priority < ProcTable[getpid()].priority){
		pDebug(2," <- spawnReal(): Kid[%d] priority (MboxID = [%d]) is HIGHER , blocking parent[%d]...\n",kidPid,ProcTable[getpid()%MAXPROC].mBoxID,getpid());
	}else{
		pDebug(2," <- spawnReal(): Kid[%d] priority (MboxID = [%d]) is LOWER , continue parent[%d]...\n",kidPid,ProcTable[kidPid%MAXPROC].mBoxID,getpid());
	}
	pDebug(3," <- spawnReal() - END pid=[%d] kidpid=[%d]\n",getpid(),kidPid);
	
	// Return pid of new process
	return kidPid;
}	
 
/*****************************************************************************
 *  Routine:  spawnLaunch
 *  Description:
 *	 			Analougous to Phase 1 launch()
 ******************************************************************************/
int spawnLaunch(char * func){ //
	int msg;
	int launchPID = getpid()%MAXPROC; // DO NOT REMOVE - if getpid() called after purUserMode() halts USLOSS.
	pDebug(2," <- spawnLaunch() - START: startFunc = [%p]\n",func);
	if(ProcTable[launchPID].mBoxID == -1){
		
		// If mBoxID == -1, mailbox has not been attached to process, attache one.
		//creates a zero slot mailbox
		ProcTable[launchPID].mBoxID = MboxCreate(0,100);
		pDebug(2," <- spawnLaunch() - Calling PID=[%d], MBox is NULL, Creating MboxID = [%d]\n",getpid(),ProcTable[launchPID].mBoxID);
		
		//If the mailbox has not yet been initialized, this means that spawn launch of
		//the parent process has not yet finished, because the child is of higher priority.
		//Therefore block at this point so that the necessary actions of the parent 
		//process can occur
		MboxReceive(ProcTable[launchPID].mBoxID,&msg,sizeof(void*));
		
		// Update the launchPid after blocking
		launchPID = getpid()%MAXPROC;
		pDebug(2," <- spawnLaunch() - After MboxReceive mboxID = [%d] launchPID=[%d]\n",ProcTable[launchPID].mBoxID,launchPID);
	}else{
		pDebug(3," <- spawnLaunch() - MboxID = [%d]\n",ProcTable[getpid()%MAXPROC].mBoxID);
	}
	
	//If the process is zapped, you no longer want it to run. Therefore call quit on the process
	if(isZapped()){
		pDebug(3, " <- spawnLaunch() - pid[%d] ZAPPED, call quit\n",getpid());
		quit(-3);
	}
	
	//This status means that a child that is dead is trying to run, have the process quit
	if(ProcTable[launchPID].status == 1){
		pDebug(2, " <- spawnLaunch() - DEAD CHILD Trying to LAUNCH!!!\n",getpid());
		quit(-3);
	}else{
			
		// Switch to usermode to run user code.
		putUserMode();
		
		pDebug(2,"\n <- spawnLaunch(): Launching_PID = [%d] launchName = [%s] launchFunc = [%p] mBoxID = [%d]\n",launchPID,ProcTable[launchPID].name,ProcTable[launchPID].startFunc,ProcTable[launchPID].mBoxID);
		
		// launch process
		int result = ProcTable[launchPID].startFunc(ProcTable[launchPID].arg);
		pDebug(2," <- spawnLaunch() - After Launch: result = [%d]\n",result);
		
		//Terminate the current process after it is finished running. This is so that 
		//quit is called on the process
		Terminate(ProcTable[launchPID].returnStatus);
		pDebug(2," <- spawnLaunch() - AFTER TERMINATE\n");
		return -3; // probly wrong, had to change prototype to int spawnLaunch(), spawnLaunch is called however.
		}
	return -3;
}

/*****************************************************************************
 *  Routine:  waitNotLinux
 *
 *  Wait for a child process to terminate.
 *  Output
 *  	arg1: process id of the terminating child.
 *  	arg2: the termination code of the child.
 *****************************************************************************/
void waitNotLinux(systemArgs *args){
	pDebug(3," <- waitNotLinux()\n");
	int status;
	int pid = waitReal(&status);
	args->arg1 = (void*)(long)pid;
	args->arg2 = (void*)(long)status;
}

/*****************************************************************************
 *  Routine:  waitReal - Like join, makes sure kid has not quit
 *
 *  Description: This is the call entry to waitNotLinux for a child completion
 *
 *  Arguments:    int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 *****************************************************************************/
int waitReal(int *status){ 
	pDebug(2," <- waitReal(): Begin\n");
	int joinVal;
	
	//call join on the current process so that it waits for 
	//its child process to quit
	int pid = join(&joinVal);
	if(ProcTable[pid%MAXPROC].status == 1){
		pDebug(2," <- waitReal(): Child is DEAD, pidRetunred = [%d]\n",pid);
	}
    *status = ProcTable[pid%MAXPROC].returnStatus;
	pDebug(2," <- waitReal(): End - CurrentPID[%d] joinPID[%d] joinVal[%d]\n",getpid(),pid,joinVal);
	//putUserMode();
    return pid;
} /* end of Wait */

/*****************************************************************************
 *	Routine:  terminate
 *
 *	Terminates the invoking process and all of its children, and synchronizes
 *  with its parent’s Wait system call. Processes are terminated by zap’ing
 *  them.When all user processes have terminated, your operating system should
 *  shut down. Thus, after start3 terminates (or returns) all user processes
 *  should have terminated. Since there should then be no runnable or blocked
 *  processes, the kernel will halt.
 *  Input
 *  	arg1: termination code for the process.
 *****************************************************************************/
void terminate(systemArgs *args){
	pDebug(3," <- terminate()\n");
	int terminateResult = terminateReal(getpid(),(long)args->arg1);
	args->arg1 = (void*)(long)terminateResult;
}

/*****************************************************************************
 *	Routine:  terminateReal
 *
 *	Terminates the invoking process and all of its children, and synchronizes
 *  with its parent’s Wait system call. Processes are terminated by zap’ing
 *  them.When all user processes have terminated, your operating system should
 *  shut down. Thus, after start3 terminates (or returns) all user processes
 *  should have terminated. Since there should then be no runnable or blocked
 *  processes, the kernel will halt.
 *  Input
 *  	pid: pid of the process to terminate
 *		returnStatus: return status for the calling process
 *****************************************************************************/
int terminateReal(int pid,long returnStatus){
	//set the return status in the process table 
	ProcTable[pid%MAXPROC].returnStatus = returnStatus;
	
	//if the process has already been zapped, return 0
	if(isZapped())
		return 0;
	
	//Checks to see if the process being terminated has children
	//if the process does have children, then you need to zap all the 
	//children along with the calling process
	if(ProcTable[pid%MAXPROC].childList.count >0){
		pDebug(2," <- terminateReal(): pid[%d] has [%d] children:\n",pid,ProcTable[pid%MAXPROC].childCount);
		if(debugVal>0)
			printQ(ProcTable[pid%MAXPROC].childList);
		
		//This loop is used to zap all the children of the calling process
		while(ProcTable[pid%MAXPROC].childList.count >0){

			//gets the child of the current process from the queue
			procPtr tempChild = pop(&ProcTable[pid%MAXPROC].childList);
			// zap 1 child at a time, then when child quits, parent get reawoke when child realizes its zapped, then zap next child, and so forth
			if(tempChild->status == CHILD_ALIVE){
				pDebug(2," <- ZAPPING --- Child[%d]\n",tempChild->pid);
				tempChild->status = CHILD_ZAPPED;
				ProcTable[pid%MAXPROC].childCount--;
				zap(tempChild->pid);
				tempChild->mBoxID = -1;
			}
			pDebug(3," <- terminateReal(): after zap\n");

		}
			//quit(returnStatus); // Phase 1 must be notified process has quit.
	}else{
		
		//This occurs when the current process has no children.
		//Set the ststus of the process appropriately
		pDebug(2," <- terminateReal(): pid[%d] has 0 children\n",pid);
		ProcTable[pid%MAXPROC].returnStatus = returnStatus;
		ProcTable[pid%MAXPROC].status = CHILD_DEAD;
		
		// Remove child from child list of parent as it has quit normally
		removeQ(&ProcTable[ProcTable[pid%MAXPROC].parentPID%MAXPROC].childList,pid%MAXPROC);

		pDebug(2," <- terminateReal(): END - pid[%d] returnStatus[%d]\n",pid,returnStatus);
		
		//Free MailBox and Process from ProcTable
		ProcTable[pid%MAXPROC].mBoxID = -1;
		ProcTable[pid%MAXPROC].pid = -1;
		quit(-111); // Phase 1 must be notified process has quit.
	}
	putUserMode();
	return -222;
}

/*****************************************************************************
 *	Routine:  semCreate
 *
 *  Creates a user-level semaphore.
 *  Input
 *  	arg1: initial semaphore value.
 *  Output
 *  	arg1: semaphore handle to be used in subsequent semaphore system calls.
 *  	arg4: -1 if initial value is negative or no semaphores are available; 0
 *		otherwise.
 *****************************************************************************/
void semCreate(systemArgs *args){
	pDebug(1," <- semCreate()\n");
    int semVal = semCreateReal((long)args->arg1);
	if(semVal == -1 || (long)args->arg1 <0)
		args->arg4 = (void*)(long)-1;
	else{
		args->arg1 = (void *)(long)semVal;
		args->arg4 = (void*)(long)0;
	}
	putUserMode();
}

/*****************************************************************************
 *	Routine:  semCreateReal
 *
 *  Creates a user-level semaphore.
 *  Input
 *  	Initial Value of the semaphore that is being created
 *	Output
 *		The return value is the id of the semaphore created
 *      Returns -1 if semaphore had incorrect values or is full
 *****************************************************************************/
int semCreateReal(int initialVal){
	pDebug(1,"semCreateReal(): initialVal = [%d]\n",initialVal);
	int returnVal = -1;
	
	//initialize all the values of the sem created
	//this includes a mailbox of the size of the 
	//semaphore that is being used
	for(i=0;i<MAXSEMS;i++){
		if(SemTable[i].mBoxID == -1){
			SemTable[i].initialVal = initialVal;
			SemTable[i].currentVal = 0;
			SemTable[i].processPID = getpid()%MAXPROC;
			SemTable[i].mBoxID = MboxCreate(initialVal,0);
			SemTable[i].mutexID = MboxCreate(1,0);
			returnVal = i;
			break;
		}
	}
	putUserMode();
	return returnVal;
}

/*****************************************************************************
 *	Routine:  semP
 *
 *  Performs a “P” operation on a semaphore.
 *  Input
 * 		 arg1: semaphore handle.
 *  Output
 * 		 arg4: -1 if semaphore handle is invalid, 0 otherwise.
 *****************************************************************************/
void semP(systemArgs *args){
	pDebug(1," <- semP()\n");
	if(SemTable[(long)args->arg1].mBoxID != -1){
		semPReal((long)args->arg1);
		args->arg4 = (void*)(long)0;
	}else
		args->arg4 = (void*)(long)-1;
}

/*****************************************************************************
 *	Routine:  semPReal
 *
 *  Performs a “P” operation on a semaphore.
 *  Input
 * 		 The id of the semaphore to perform the p operation on
 *****************************************************************************/
void semPReal(int semID){
	int recieveResult;
	

	
	pDebug(1," <- semPReal(): -START- CurrentPID[%d] semMBoxID[%d] sem.currentVal = [%d] sem.initialVal = [%d]\n",getpid()%MAXPROC,SemTable[semID].mBoxID,SemTable[semID].currentVal,SemTable[semID].initialVal);
	SemTable[semID].processPID = getpid()%MAXPROC;

	//The semaphore is not currently in use, do a send to the mailbox in the semaphore
	//So that the process performing the p operation sets the mutex to in use
	if(SemTable[semID].currentVal <= 0){
	
		// Get process mboxID currently using Semaphore (bookkeeping only)
		SemTable[semID].processPID = getpid()%MAXPROC;
	
		MboxSend(SemTable[semID].mBoxID,&recieveResult,0);
		// If MboxID == -1: The semaphore has benn released, and this child cannot continue to execute.
		if(SemTable[semID].mBoxID == -1){
			//check if the process was zapped
			if(isZapped())
				terminateReal(getpid()%MAXPROC,0);
			else
				terminateReal(getpid()%MAXPROC,1);	
		}
	}

	
	// The semaphore is currently in use so we will block here
	if(SemTable[semID].currentVal >=0){
		
		// get mutex
		MboxSend(SemTable[semID].mutexID,&recieveResult,0);
		
		// If we are going to block on this send, update process status to blocked for semFree()
		if(SemTable[semID].currentVal+1 >= SemTable[semID].initialVal){
			pDebug(1,"semPReal(): Adding to blockList\n");
			push(&SemTable[semID].blockList,(long long)time(NULL),&ProcTable[SemTable[semID].processPID]);
			ProcTable[SemTable[semID].processPID].PVstatus = STATUS_PV_BLOCKED;
		}else{
			SemTable[semID].currentVal--;
		}
		
		// release mutex
		MboxReceive(SemTable[semID].mutexID,&recieveResult,0);
		
		if(debugVal>2){
			ds3();
			dp3();
		}	
		pDebug(1," <- semPReal(): -After MboxSend- (SemTable[semID].mBoxID,&recieveResult,0);\n");
	}else{
			
	}
	putUserMode();
	

}

/*****************************************************************************
 *	Routine:  semV
 *
 *  Performs a “V” operation on a semaphore.
 *  Input
 *      arg1: semaphore handle.
 *  Output
 *      arg4: -1 if semaphore handle is invalid, 0 otherwise.
 *****************************************************************************/
void semV(systemArgs *args){
	pDebug(1," <- semV()\n");
	if(SemTable[(long)args->arg1].mBoxID != -1){
		semVReal((long)args->arg1);
		args->arg4 = (void*)(long)0;
	}else
		args->arg4 = (void*)(long)-1;
}

/*****************************************************************************
 *	Routine:  semVReal
 *
 *  Performs a “v” operation on a semaphore.
 *  Input
 * 		 The id of the semaphore to perform the v operation on
 *****************************************************************************/
void semVReal(int semID){
	int sendResult;

	pDebug(1," <- semVReal(): -START- CurrentPID[%d] semMBoxID[%d] sem.currentVal = [%d] sem.initialVal = [%d]\n",getpid()%MAXPROC,SemTable[semID].mBoxID,SemTable[semID].currentVal,SemTable[semID].initialVal);
	
	// Get process mboxID currently using Semaphore (bookkeeping only)
	SemTable[semID].processPID = getpid()%MAXPROC;
		
	// get mutex
	MboxSend(SemTable[semID].mutexID,&sendResult,0);

	// check to see if the process will be blocked on the v process of not
	// set pvStatus appropriately based on this
	if(SemTable[semID].currentVal <= SemTable[semID].initialVal){
		if (SemTable[semID].blockList.count>0){
			procPtr tempProc = pop(&SemTable[semID].blockList);
			ProcTable[tempProc->pid].PVstatus = STATUS_NOT_PV_BLOCKED;
		}
	}else		
		SemTable[semID].currentVal++;
	
	// release mutex
	MboxReceive(SemTable[semID].mutexID,&sendResult,0);
	
	if(debugVal>2){
		ds3();
		dp3();
	}
	
	//perform the receive which is the equivalent of decrementing the semaphore.
	MboxReceive(SemTable[semID].mBoxID,&sendResult,0);
	pDebug(1," <- semVReal(): After MboxReceive(SemTable[semID].mBoxID,&sendResult,0);");
	putUserMode();
}

/*****************************************************************************
 *	Routine:  semFree
 *
 *  Frees a semaphore.
 *  Input
 *  arg1: semaphore handle.
 *  Output
 *  	arg4: -1 if semaphore handle is invalid, 1 if there were processes 
 * 		blocked on the semaphore, 0 otherwise.
 *  Any process waiting on a semaphore when it is freed should be terminated
 *  using the equivalent of the Terminate system call.
 *****************************************************************************/
void semFree(systemArgs *args){
	pDebug(1," <- semFree()\n");
	if(SemTable[(long)args->arg1].mBoxID != -1){
		if(semFreeReal((long)args->arg1) == 0){
			args->arg4 = (void*)(long)0;
		}else{
			args->arg4 = (void*)(long)1;
		}			
	}else{
		args->arg4 = (void*)(long)-1;
	}
	putUserMode();
}

/*****************************************************************************
 *	Routine:  semFreeReal
 *
 *  Frees a semaphore.
 *  Input
 *  semID: Id of the semaphore that is going to be freed
 *  Output
 *  	-1 if semaphore handle is invalid, 1 if there were processes 
 * 		blocked on the semaphore, 0 otherwise.
 *  Any process waiting on a semaphore when it is freed should be terminated
 *  using the equivalent of the Terminate system call.
 *****************************************************************************/
int semFreeReal(int semID){
	int returnVal = 0;
	pDebug(1," <- semFreeReal()\n");
	
	//there are no processes in the blocked list, return 0
	if(SemTable[semID].blockList.count==0)
		returnVal = 0;
	else{
		//Return 1, there are blocked processes on this sem still
		returnVal = 1;
	}

	//Now that the semaphore is no longer in use, set all the variables in the 
	//sem to indicate this
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

/*****************************************************************************
 *	Routine:  getTimeofDay
 *
 *  Returns the value of USLOSS time-of-day clock.
 *  Output
 *  	arg1: the time of day.
 *****************************************************************************/
void getTimeofDay(systemArgs *args){
	pDebug(3," <- getTimeofDay()\n");
	int status;
	if (USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status) == 0)
		args->arg1 = (void*)(long)status;
	else
		args->arg1 = (void*)(long)-1; //zapped
	putUserMode();
}

/*****************************************************************************
 *	Routine:  cPUTime
 *
 *  Returns the CPU time of the process (this is the actual CPU time used, not just the time since the
 *  current time slice started).
 *  Output
 *  arg1: 
 *  	the CPU time used by the currently running process.
 *****************************************************************************/
void cPUTime(systemArgs *args){
	pDebug(3," <- cPUTime()\n");
	args->arg1 = (void*)(long)readtime();
	putUserMode();
}

/*****************************************************************************
 *	Routine:  getPID
 *
 *  Returns the process ID of the currently running process.
 *  Output
 *  	arg1: the process ID.
 *****************************************************************************/
void getPID (systemArgs *args){
	pDebug(3," <- getPID()\n");
	args->arg1 = (void*)(long)getpid();
	putUserMode();
}

/*****************************************************************************
 *      Initialize systemCallVec calls
 *****************************************************************************/
 void intializeSysCalls(){
	 
	// Start by initially setting all the syscall vecs to a nullsys
    for (int i = 0; i < MAXSYSCALLS; i++) {
        systemCallVec[i] = nullsys3;
    }
	
	// set the appropriate system call handlers to their place in the system vector
	// that way the appropriate error handler is called when an interrupt occurs
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

 /*****************************************************************************
 *      Initialize nullsys system call handler calls
 *		Input:
 *			Array of arguments
 *****************************************************************************/
void nullsys3(systemArgs *args) {
    USLOSS_Console("nullsys3(): Invalid syscall %d. Halting...\n", args->number);
	// Terminate instead of USLOSS_Halt(1) from phase1
	terminate(args);
} /* nullsys3 */

/*****************************************************************************
 *     pDebug - Outputs a printf-style formatted string to stdout
 *****************************************************************************/
int pDebug(int level, char *fmt, ...) {
	 va_list args;
	 va_start(args, fmt);
     if(debugVal >= level)
        USLOSS_VConsole(fmt,args);
	 return 1;
} /* pDebug */

/*****************************************************************************
 *     putUserMode - puts the OS into usermode with the appropriate call to 
 *		psrSet
 *****************************************************************************/
void putUserMode(){
	int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
	pDebug(3," <- putUserMode(): Result = %d\n",result);
}

/*****************************************************************************
 *     dp3 - Used to output a formatted version of the process table to the 
 *		usloss console
 *****************************************************************************/
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

/*****************************************************************************
 *     ds3 - Used to output a formatted version of the semaphore table to the 
 *		usloss console
 *****************************************************************************/
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