
#include <usloss.h>
#include <usyscall.h>
#include "libuser.h"
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"
#include "providedPrototypes.h"
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h>

int     debugVal = 0; // 0 == off to 3 == most debug info

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
int 	mainSemaphore;
int		i;
static int	ClockDriver(char *);
static int	DiskDriver(char *);
static int	TermDriver(char *);
procTable ProcTable[MAXPROC];
MinQueue SleepList;
MinQueue DriveSizeList;
int DiskDrives[USLOSS_DISK_UNITS];
int Terminals[USLOSS_TERM_UNITS];

void start3(void){
	pDebug(1," <- start3(): start\n");
    char	name[128];
    char    termbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;
	char 	buf[100];
	termbuf[0] = '\0';
    /*
     * Check kernel mode here.
     */
	check_kernel_mode("start3");
	
	
	/*****************************************************
	*             Initialize Phase 4 data structures
	*****************************************************/
	//Initialize the process table here
	for (i=0;i<MAXPROC;i++){
		ProcTable[i].pid = -1;
		ProcTable[i].name[0] = '\0';
		ProcTable[i].status = -1;
		ProcTable[i].PVstatus = -1;
		ProcTable[i].sleepAt = -1;
		ProcTable[i].sleepDuration = -1;
		ProcTable[i].sleepWakeAt = -1;
		ProcTable[i].semID = semcreateReal(0);
		ProcTable[i].mboxID = MboxCreate(0,0);
		ProcTable[i].disk_sector_size = -1;
		ProcTable[i].disk_track_size = -1;
		ProcTable[i].disk_size = -1;
	}
	
	//Initialize Sleeplist
		intialize_queue2(&SleepList);
	
	//Initialize Sleeplist
		intialize_queue2(&DriveSizeList);
		
	//Initialize SystemCalls
		intializeSysCalls();
		
		
    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    mainSemaphore = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
		USLOSS_Console("start3(): Can't create clock driver\n");
		USLOSS_Halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    sempReal(mainSemaphore);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
		sprintf(name, "DiskDriver %d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
		//DiskDrives[i] = pid;
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
		sempReal(mainSemaphore);
    }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(buf, "%d", i);
		sprintf(name, "TermDriver %d", i);
        pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
		Terminals[i] = pid;
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
		sempReal(mainSemaphore);
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);
	pDebug(1," <- start3(): after start4() \n");

    /*
     * Zap the device drivers
     */
	//dumpProcesses();
	//dp4();
	// zap clock driver
    zap(clockPID);  
	
	if(debugVal>1)
		dumpProcesses();
	
	// zap disk drives
	for (i=0;i<USLOSS_DISK_UNITS;i++){
		pDebug(1," <- zapping disk[%d] pid[%d]...\n",i,DiskDrives[i]);
		semvReal(ProcTable[DiskDrives[i]].semID);
		zap(DiskDrives[i]);  // clock driver
	}

	// zap terminals
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		pDebug(1," <- zapping terminal[%d] pid[%d]...\n",i,Terminals[i]);
		semvReal(ProcTable[Terminals[i]].semID);
		zap(Terminals[i]);  // clock driver
	}
	
    // eventually, at the end:
    quit(0);
    
}

/*************************************************************************
 *
 *                           System Calls
 *
 ************************************************************************/

/*****************************************************************************
 *      Initialize systemCallVec calls
 *****************************************************************************/
 void intializeSysCalls(){
	 
	// Start by initially setting all the syscall vecs to a nullsys4
	//  for (int i = 0; i < MAXSYSCALLS; i++) {
	//      systemCallVec[i] = nullsys4;
	// }
	
	// set the appropriate system call handlers to their place in the system vector
	// that way the appropriate error handler is called when an interrupt occurs
	systemCallVec[SYS_SLEEP] = sleep;
	systemCallVec[SYS_DISKREAD] = diskRead;
	systemCallVec[SYS_DISKWRITE] = diskWrite;
	systemCallVec[SYS_DISKSIZE] = diskSize;
	systemCallVec[SYS_TERMREAD] = termRead;
	systemCallVec[SYS_TERMWRITE] = termWrite;
 } 
 
/******************************************************************************
 *  Routine: Sleep
 *
 *  Description: This is the call entry point for timed delay.
 *  Arguments:    int seconds -- number of seconds to sleep
 *  Return Value: 0 means success, -1 means error occurs
 *
 *****************************************************************************/
int Sleep(int seconds){
	pDebug(1," <- Sleep(): start \n");
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *)(long) seconds;
    USLOSS_Syscall((void *)(long) &sysArg);
    return (long) sysArg.arg4;
} /* end of Sleep */

void sleep(USLOSS_Sysargs *args){
	pDebug(1," <- sleep(): start \n");
	int returnVal = sleepReal((long)args->arg1);
	args->arg1 = (void*)(long)returnVal;
}

int sleepReal(long sleepDuration){
	pDebug(1," <- sleepReal(): start \n");
	if(sleepDuration < 0)
		return -1; // Fail
	else{
		int sleepAt;
		gettimeofdayReal(&sleepAt);
		int sleepWakeAt = sleepAt + sleepDuration*1000000;
		
		ProcTable[getpid()%MAXPROC].pid = getpid();
		ProcTable[getpid()%MAXPROC].sleepDuration = sleepDuration*1000000;
		ProcTable[getpid()%MAXPROC].sleepWakeAt = sleepWakeAt;
		ProcTable[getpid()%MAXPROC].sleepAt = sleepAt;
		push(&SleepList,sleepWakeAt,&ProcTable[getpid()%MAXPROC]);
		sempReal(ProcTable[getpid()%MAXPROC].semID);
	}
	return 0; // Success
}



/******************************************************************************
 *  Routine:  DiskRead
 *  Description: This is the call entry point for disk input.
 *  Arguments:    void* dbuff  -- pointer to the input buffer
 *                int   track  -- first track to read 
 *                int   first -- first sector to read
 *		  int	sectors -- number of sectors to read
 *		  int   unit   -- unit number of the disk
 *                int   *status    -- pointer to output value
 *                (output value: completion status)
 *  Return Value: 0 means success, -1 means error occurs
 *
******************************************************************************/
int DiskRead(void *dbuff, int track, int first, int sectors, int unit, int *status){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *)(long) sectors;
    sysArg.arg3 = (void *)(long) track;
    sysArg.arg4 = (void *)(long) first;
    sysArg.arg5 = (void *)(long) unit;
    USLOSS_Syscall((void *)(long) &sysArg);
    *status = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of DiskRead */

void diskRead(USLOSS_Sysargs *args){
		pDebug(1," <- diskRead(): start \n");
	
}

int diskReadReal(){
	return -1;
}

/******************************************************************************
 *  Routine:  Sys_DiskWrite
 *  Description: This is the call entry point for disk output.
 *  Arguments:    void* dbuff  -- pointer to the output buffer
 *                int   track  -- first track to write
 *                int   first -- first sector to write
 *		  int	sectors -- number of sectors to write
 *		  int   unit   -- unit number of the disk
 *                int   *status    -- pointer to output value
 *                (output value: completion status)
 *  Return Value: 0 means success, -1 means error occurs
******************************************************************************/
int DiskWrite(void *dbuff, int track, int first, int sectors, int unit, int *status){
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *)(long) sectors;
    sysArg.arg3 = (void *)(long) track;
    sysArg.arg4 = (void *)(long) first;
    sysArg.arg5 = (void *)(long) unit;
    USLOSS_Syscall((void *) &sysArg);
    *status = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of DiskWrite */

void diskWrite(USLOSS_Sysargs *args){
		pDebug(1," <- diskWrite(): start \n");
}

int diskWriteReal(){
	return -1;
}

/******************************************************************************
 *  Routine:  Sys_DiskSize
 *
 *  Description: Return information about the size of the disk.
 *
 *  Arguments:    int   unit  -- the unit number of the disk 
 *                int   *sector -- bytes in a sector
 *		  		  int	*track -- number of sectors in a track
 *                int   *disk  -- number of tracks in the disk
 *                (output value: completion status)
 *
 *  Return Value: 0 means success, -1 means invalid parameter
 *****************************************************************************/
int DiskSize(int unit, int *sector, int *track, int *disk){
	USLOSS_Sysargs sysArg;

	CHECKMODE;
	sysArg.number = SYS_DISKSIZE;
	sysArg.arg1 = (void *)(long) unit;
	USLOSS_Syscall((void *)(long) &sysArg);
	*sector = (long) sysArg.arg1;
	*track = (long) sysArg.arg2;
	*disk = (long) sysArg.arg3;
	return (long) sysArg.arg4;
} /* end of DiskSize */

void diskSize(USLOSS_Sysargs *args){
	pDebug(1," <- diskSize(): start \n");
	int unit = (long)args->arg1;
	int returnVal = diskSizeReal(unit);
	if(returnVal == 0){
		args->arg1 = (void*)(long)ProcTable[DiskDrives[unit]].disk_sector_size;
		args->arg2 = (void*)(long)ProcTable[DiskDrives[unit]].disk_track_size;
		args->arg3 = (void*)(long)ProcTable[DiskDrives[unit]].disk_size;
	}
	args->arg4 = (void*)(long)returnVal;
}
	
int diskSizeReal(int unit){
	pDebug(1," <- diskSizeReal(): start \n");
	if(unit <0 || unit >1)
		return -1;
	push(&DriveSizeList,(long long)time(NULL),&ProcTable[getpid()%MAXPROC]);
	
	// Wake up disk.
	semvReal(ProcTable[DiskDrives[unit]].semID);
	
	// Block Calling Process.
	sempReal(ProcTable[getpid()%MAXPROC].semID);
	return 0;
}

/******************************************************************************
 *  Routine:  TermRead
 *
 *  Description: This is the call entry point for terminal input.
 *
 *  Arguments:    char *buff    -- pointer to the input buffer
 *                int   bsize   -- maximum size of the buffer
 *                int   unit_id -- terminal unit number
 *                int  *nread      -- pointer to output value
 *                (output value: number of characters actually read)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *****************************************************************************/
int TermRead(char *buff, int bsize, int unit_id, int *nread){
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = (void *)(long) buff;
    sysArg.arg2 = (void *)(long) bsize;
    sysArg.arg3 = (void *)(long) unit_id;
    USLOSS_Syscall((void *)(long) &sysArg);
    *nread = (long) sysArg.arg2;
    return (long) sysArg.arg4;
} 

void termRead(USLOSS_Sysargs *args){
		pDebug(1," <- diskSizeReal(): start \n");
	
}

int termReadReal(){
	return -1;
}

/******************************************************************************
 *  Routine:  TermWrite
 *
 *  Description: This is the call entry point for terminal output.
 *
 *  Arguments:    char *buff    -- pointer to the output buffer
 *                int   bsize   -- number of characters to write
 *                int   unit_id -- terminal unit number
 *                int  *nwrite      -- pointer to output value
 *                (output value: number of characters actually written)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *****************************************************************************/
int TermWrite(char *buff, int bsize, int unit_id, int *nwrite){
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = (void *)(long) buff;
    sysArg.arg2 = (void *)(long) bsize;
    sysArg.arg3 = (void *)(long) unit_id;
    USLOSS_Syscall((void *) &sysArg);
    *nwrite = (long) sysArg.arg2;
    return (long) sysArg.arg4;
} 

void termWrite(USLOSS_Sysargs *args){
		pDebug(1," <- termWrite(): start \n");
	
}

int termWriteReal(){
	return -1;
}


/*************************************************************************
*
*                           DRIVERS
*
*************************************************************************/
static int DiskDriver(char *arg){
    pDebug(1," <- DiskDriver(): start \n");
    int result = 0;
    int status = 0;
	int unit = atoi(arg);
	
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
	
	pDebug(1," <- DiskDriver(): starting disk [%d]...\n",unit);
	
	//Enable Inturrupts
    enableInterrupts();
	
	// Initialize Disk Unit arg
	DiskDrives[unit] = getpid();
	ProcTable[DiskDrives[unit]].pid = getpid();
	ProcTable[DiskDrives[unit]].disk_sector_size = USLOSS_DISK_SECTOR_SIZE;
	ProcTable[DiskDrives[unit]].disk_track_size = USLOSS_DISK_TRACK_SIZE;
	ProcTable[DiskDrives[unit]].disk_size = USLOSS_DISK_TRACK_SIZE * (unit+1); // Makes no sense, gathered from 
   
   // Infinite loop until we are zap'd
    while(! isZapped()) {
		pDebug(1," <- DiskDriver(): Before block on disk [%d]...\n",unit);
		sempReal(ProcTable[DiskDrives[unit]].semID);
		if(DriveSizeList.count > 0){
			dp4();
			procPtr temp = pop(&DriveSizeList);
			pDebug(1," <- DiskDriver(): unblocking calling process [%d]...\n",temp->pid);
			semvReal(temp->semID);

			//result = waitDevice(USLOSS_DISK_DEV, unit, &status);
		}



		if (result != 0) {
			return 0;
		}
		/*
		 * Compute the current time and wake up any processes
		 * whose time has come.
		 */
		 
		//while(1){
		//}

	}
			//semvReal(ProcTable[getpid()%MAXPROC].semID);
	pDebug(1," <- DiskDriver(): end \n");
	return status;
}

static int TermDriver(char *arg){
    pDebug(1," <- TermDriver(): start \n");
    int result = 0;
    int status = 0;
	int unit = atoi(arg);
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
    int enableInturruptResult = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
	if (enableInturruptResult == 1)
		USLOSS_Halt(1);
	

    // Infinite loop until we are zap'd
    while(! isZapped()) {
		sempReal(ProcTable[Terminals[unit]].semID);
		//result = waitDevice(USLOSS_TERM_DEV, unit, &status);
		if (result != 0) {
			return 0;
		}
		/*
		 * Compute the current time and wake up any processes
		 * whose time has come.
		 */
	//	while(1){
	//	}
	}
	
	pDebug(1," <- TermDriver(): end \n");
	return status;
}

static int ClockDriver(char *arg){
	pDebug(1," <- ClockDriver(): start \n");
    int result;
    int status;
	int time;
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
    int enableInturruptResult = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
	if (enableInturruptResult == 1)
		USLOSS_Halt(1);
	
    // Infinite loop until we are zap'd
    while(! isZapped()) {
		result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
		if (result != 0) {
			return 0;
		}
		/*
		 * Compute the current time and wake up any processes
		 * whose time has come.
		 */
		//dp4();
		//printQ(SleepList);

		gettimeofdayReal(&time); 
		while(SleepList.count>0 && ((peek(SleepList)->sleepWakeAt)) <= time){
			procPtr temp = pop(&SleepList);
			semvReal(temp->semID);
			pDebug(1," <- ClockDriver(): waking up = pid[%d]\n",temp->pid); 
			
		}

	}
	pDebug(1," <- ClockDriver(): end \n");
	return status;
}


/*************************************************************************
*
*                           UTILITIES
*
*************************************************************************/


 /*****************************************************************************
 *      nullsys4 - Initialize nullsys system call handler calls
 *		Input:
 *			Array of arguments
 *****************************************************************************/
void nullsys4(USLOSS_Sysargs *args) {
    USLOSS_Console("nullsys4(): Invalid syscall %d. Halting...\n", args->number);
	// Terminate instead of USLOSS_Halt(1) from phase1
	terminateReal(getpid());
} /* nullsys3 */

int enableInterrupts(){
	int enableInturrupts = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
	if (enableInturrupts == 1){
		USLOSS_Console("Enable Inturrupts failed, haulting...\n");
		USLOSS_Halt(1);
	}
	return enableInturrupts;
}
/*****************************************************************************
 *     putUserMode - puts the OS into usermode with the appropriate call to 
 *		psrSet
 *****************************************************************************/
void putUserMode(){
	int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
	pDebug(3," <- putUserMode(): Result = %d\n",result);
}

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

/*************************************************************************
*   check_kernel_mode - Checks if the PsrBit is set to kernal mode
*   	when called by process, if not halts.
*************************************************************************/
int check_kernel_mode(char *procName){
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        USLOSS_Console("Process [%s] pid[%d] to call a function in user mode...halting.\n",procName, getpid());
        USLOSS_Halt(1);
        return 0;
    }else
        return 1;
} /* check_kernel_mode */

/*****************************************************************************
 *     dp4 - Used to output a formatted version of the process table to the 
 *		usloss console
 *****************************************************************************/
void dp4(){
    USLOSS_Console("\n---------------------------PROCESS TABLE--------------------------\n");
    USLOSS_Console(" PID  Name     Status  SleepAt  SleepDur. SleepWakeAt SemID MboxID\n");
    USLOSS_Console("------------------------------------------------------------------\n");
	for( i= 0; i< MAXPROC;i++){
		if(ProcTable[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %s[%-6s] %-0s[%d] %-3s[%-2d] %-3s[%-2d] %-5s[%-2d] %-6s[%-2d] %-1s[%-2d]\n","",ProcTable[i].pid,"", ProcTable[i].name,"",ProcTable[i].status,"",ProcTable[i].sleepAt,"",ProcTable[i].sleepDuration,"",ProcTable[i].sleepWakeAt,"",ProcTable[i].semID,"",ProcTable[i].mboxID);
		}	  
	}
    USLOSS_Console("------------------------------------------------------------------\n");    
}