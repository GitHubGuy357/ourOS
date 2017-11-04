
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
#include <string.h>

int     debugVal = 0; // 0 == off to 3 == most debug info

/*****************************************************
*             Globals
*****************************************************/
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);
int 	mainSemaphore;
int		i;
procTable ProcTable[MAXPROC];
diskTable DiskTable[USLOSS_DISK_UNITS];
termTable TermTable[USLOSS_TERM_UNITS];
termTable TermReadTable[USLOSS_TERM_UNITS];
termTable TermWriteTable[USLOSS_TERM_UNITS];
MinQueue SleepList;
char diskOps[5][20] = {"USLOSS_DISK_READ","USLOSS_DISK_WRITE","USLOSS_DISK_SEEK","USLOSS_DISK_TRACKS","USLOSS_DISK_SIZE"};

/*****************************************************
*             ProtoTypes
*****************************************************/
static int	ClockDriver(char *);
static int	DiskDriver(char *);
static int	TermDriver(char *);
static int	TermReader(char *);
static int	TermWriter(char *);

void start3(void){
	pDebug(2," <- start3(): start\n");
    char	name[128];
    char    termbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;
	char 	buf[100];
	buf[0] = termbuf[0];
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
		ProcTable[i].diskOp = -1;
		ProcTable[i].dbuff = NULL;
		ProcTable[i].track = -1;
		ProcTable[i].first = -1;
		ProcTable[i].sectors = -1;
		ProcTable[i].unit = -1;
	}
	
	// Initialize Drive Table
	for (i=0;i<USLOSS_DISK_UNITS;i++){
		DiskTable[i].pid = -1;
		DiskTable[i].disk_sector_size = -1;
		DiskTable[i].disk_track_size = -1;
		DiskTable[i].disk_size = -1;
		DiskTable[i].currentOp = -1;
		DiskTable[i].currentTrack = -1;
		DiskTable[i].currentSector = -1;
		DiskTable[i].semID = -1;
		DiskTable[i].mboxID = -1;
		DiskTable[i].drive_seek_dir = -1;
		intialize_queue2(&DiskTable[i].DriveQueueR);
		intialize_queue2(&DiskTable[i].DriveQueueL);
	}
	
	// Initialize Term Table
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		TermTable[i].pid = -1;
		TermTable[i].currentOp = -1;
		TermTable[i].semID = -1;
		TermTable[i].mboxID = -1;
		intialize_queue2(&TermTable[i].requestQueue);
	}
	
	// Initialize TermRead Table
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		TermReadTable[i].pid = -1;
		TermTable[i].type[0] = '\0';
		TermReadTable[i].currentOp = -1;
		TermReadTable[i].semID = -1;
		TermReadTable[i].mboxID = -1;
		intialize_queue2(&TermReadTable[i].requestQueue);
	}
	
	// Initialize TermWrite Table
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		TermWriteTable[i].pid = -1;
		TermWriteTable[i].currentOp = -1;
		TermWriteTable[i].semID = -1;
		TermWriteTable[i].mboxID = -1;
		intialize_queue2(&TermWriteTable[i].requestQueue);
	}
	
	// Initialize Sleeplist
		intialize_queue2(&SleepList);

	// Initialize SystemCalls
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
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
		sempReal(mainSemaphore);
    }

	 /*
     * Create terminal read device drivers.
     */
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(buf, "%d", i);
		sprintf(name, "TermRead %d", i);
        pid = fork1(name, TermReader, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driverRead %d\n", i);
            USLOSS_Halt(1);
        }
		sempReal(mainSemaphore);
    }
	
		 /*
     * Create terminal write device drivers.
     */
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(buf, "%d", i);
		sprintf(name, "TermWrite %d", i);
        pid = fork1(name, TermWriter, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driverRead %d\n", i);
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
	pDebug(2," <- start3(): after start4() \n");

    /*
     * Zap the device drivers
     */
	
	if(debugVal>1){
		dumpProcesses();
		dp4();
	}
	
	// zap disk drives
	for (i=0;i<USLOSS_DISK_UNITS;i++){
		pDebug(1," <- zapping disk[%d] pid[%d]...\n",i,DiskTable[i].pid);
		semvReal(DiskTable[i].semID);
		zap(DiskTable[i].pid);  // clock driver
	}
	
	// zap read terminals
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		pDebug(1," <- zapping read terminal[%d] pid[%d]...\n",i,TermReadTable[i].pid);
		semvReal(TermReadTable[i].semID);
		zap(TermReadTable[i].pid);
	}
	
	// zap write terminals
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		pDebug(1," <- zapping write terminal[%d] pid[%d]...\n",i,TermWriteTable[i].pid);
		semvReal(TermWriteTable[i].semID);
		zap(TermWriteTable[i].pid);
	}
	
	if(debugVal>1){
		dumpProcesses();
		dp4();
	}

	// zap terminals
	for (i=0;i<USLOSS_TERM_UNITS;i++){
		pDebug(1," <- zapping terminal[%d] pid[%d]...\n",i,TermTable[i].pid);
	//	MboxRelease(i+3);
	
		// This tell USLOSS it has a character, even though it does, to make it break out of the wait to be zapped
		int control = 7;
		int resultD = 0;
		control = USLOSS_TERM_CTRL_RECV_INT(control);
		resultD = USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void*)(long)control);
		semvReal(TermTable[i].semID);
		zap(TermTable[i].pid);
	}
	
	// zap clock driver
	pDebug(1," <- zapping clock pid[%d]...\n",clockPID);
	
	//MboxRelease(0);
    zap(clockPID);  

    // eventually, at the end:
    quit(0);
    
}

char *status2str[] = {"ready", "busy", "error"};

void print_status(int status) {
    printf("status: char: %c xmit: %s recv: %s\n", USLOSS_TERM_STAT_CHAR(status),
           status2str[USLOSS_TERM_STAT_XMIT(status)],
           status2str[USLOSS_TERM_STAT_RECV(status)]);
}

/*************************************************************************
*
*                           DRIVERS
*
*************************************************************************/
static int DiskDriver(char *arg){
    pDebug(2," <- DiskDriver(): start \n");
    int result = 0;
	int resultD = -10;
    int status;
	int unit = atoi(arg);
	int disk_size_req;
	procPtr temp = NULL;
	
	// Get disk size from USLOSS
	// If used as pointer and cast (void*) zapping segfaults, must initialize to nothing!
	USLOSS_DeviceRequest control = { .opr = USLOSS_DISK_TRACKS, .reg1 = &disk_size_req, .reg2 = NULL}; 
	resultD = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &control);
	result = waitDevice(USLOSS_DISK_DEV, unit, &status);
	
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
	
	pDebug(1," <- DiskDriver(): starting disk [%d]...\n",unit);
	
	// Enable Inturrupts
    enableInterrupts();
	
	// Initialize Disk Unit from arg
	DiskTable[unit].pid = getpid();
	DiskTable[unit].semID = semcreateReal(0);
	DiskTable[unit].mboxID = MboxCreate(0,0);
	DiskTable[unit].currentOp = -1;
	DiskTable[unit].currentTrack = 0;
	DiskTable[unit].currentSector = 0;
	DiskTable[unit].drive_seek_dir = 1; // Start seeking right.
	DiskTable[unit].disk_sector_size = USLOSS_DISK_SECTOR_SIZE;
	DiskTable[unit].disk_track_size = USLOSS_DISK_TRACK_SIZE;
	DiskTable[unit].disk_size = disk_size_req;
  
   // Infinite loop until we are zap'd
    while(! isZapped()) {
		
		// Block Disk waiting for Request.
		pDebug(1," <- DiskDriver(): Before block on disk [%d]...\n",unit);
		sempReal(DiskTable[unit].semID);
		
		// If Drive has a request in Queue, determine which request we are furfilling.
		if(DiskTable[unit].DriveQueueL.count > 0 || DiskTable[unit].DriveQueueR.count > 0){
			if(DiskTable[unit].drive_seek_dir == 1){
				if(DiskTable[unit].DriveQueueR.count != 0){
					// Added for test14, otherwise peek is null and current queue is empty and other queue is 1
					temp = peek(DiskTable[unit].DriveQueueR);
				}else 
					pDebug(1," <- DiskDriver(): ERROR: Trying to use empty DriveQueueR\n");
			}else if(DiskTable[unit].drive_seek_dir == 0){
				if(DiskTable[unit].DriveQueueL.count != 0)  {
					// Added for test14, otherwise peek is null and current queue is empty and other queue is 1
					temp = peek(DiskTable[unit].DriveQueueL);
				}else
					pDebug(1," <- DiskDriver(): ERROR: Trying to use empty DriveQueueR\n"); //continue;
			}else
				pDebug(0,"\n\n\n\nBAD TOUCH - Request to drive_seek_dir[%d]\n\n\n\n",DiskTable[unit].drive_seek_dir);
			
			pDebug(1," <- DiskDriver(): Disk[%d] Request = [%s]  from calling pid[%d]...\n",temp->unit,getOp(temp->diskOp), temp->pid);
			
			// Update Disk current track/sector for scheduling when inserting into queue.
			DiskTable[temp->unit].currentSector = temp->first;
			DiskTable[temp->unit].currentTrack =  temp->track;
				
			//Advance disk to location if read/write op
			if(temp->diskOp == USLOSS_DISK_READ || temp->diskOp == USLOSS_DISK_WRITE){
				control.opr = USLOSS_DISK_SEEK;
				control.reg1 = (void*)(long)temp->track;
				resultD = USLOSS_DeviceOutput(USLOSS_DISK_DEV, temp->unit, &control);
				result = waitDevice(USLOSS_DISK_DEV, temp->unit, &status);
				pDebug(1," <- DiskDriver(): Disk[%d] Seeking...Track[%d] FirstSector[%d] Buffer[%p]...\n",temp->unit,temp->track,temp->first,temp->dbuff);
			}
			
			// Temp variables, required or pointer points incorrectly to tests.
			int curSectorsToWrite = temp->sectors;
			void* curDbuffPtr = temp->dbuff;
			int curSector = temp->first;
			int curTrack = temp->track;

			// Loop to Write/Read through each sector requested
			while(curSectorsToWrite > 0){
				if(curSector == DiskTable[temp->unit].disk_track_size){
					pDebug(1," <- DiskDriver(): Disk[%d] Track Wrap around....curTrack[%d] & curSector[%d]",temp->unit, curTrack,curSector);
					curTrack = curTrack+1%DiskTable[temp->unit].disk_track_size; //curTrack;
					curSector = 0;
					pDebug(1," to newTrack[%d] & newSector[%d]\n",curTrack,curSector);
				
					//Advance disk to new track if wrap around
					if(temp->diskOp == USLOSS_DISK_READ || temp->diskOp == USLOSS_DISK_WRITE){
						control.opr = USLOSS_DISK_SEEK;
						control.reg1 = (void*)(long)curTrack;
						resultD = USLOSS_DeviceOutput(USLOSS_DISK_DEV, temp->unit, &control);
						result = waitDevice(USLOSS_DISK_DEV, temp->unit, &status);
						pDebug(1," <- DiskDriver(): Disk[%d] Seeking...Track[%d] Sector[%d] Buffer[%p]...\n",temp->unit,curTrack,curSector,temp->sectors,temp->dbuff);
					}
					// Update Disk current track/sector for scheduling when inserting into queue.
					DiskTable[temp->unit].currentSector = curSector;
					DiskTable[temp->unit].currentTrack =  curTrack;	
				}

				// Set control parameters for USLOSS_DeviceOutput
				control.opr = temp->diskOp;
				control.reg1 = (void*)(long)curSector; //
				control.reg2 = curDbuffPtr;
				
				// Update Disk current track/sector for scheduling when inserting into queue.
				DiskTable[temp->unit].currentSector = curSector;
				DiskTable[temp->unit].currentTrack =  curTrack;
				
				// Make request
				resultD = USLOSS_DeviceOutput(USLOSS_DISK_DEV, temp->unit, &control);
				
				pDebug(1," <- DiskDriver(): Disk[%d] waitDevice pid[%d] Before %s...curTrack[%d] curSector[%d] curDbuff[%p]\n",temp->unit,temp->pid,getOp(temp->diskOp),curTrack,curSector,curDbuffPtr);
				
				// Wait for disk inturrupt.
				result = waitDevice(USLOSS_DISK_DEV, temp->unit, &status);
				
				// If disk is zapped return.
				if (result != 0) {
					pDebug(1, "--------------Disk Quiting status[%d]--------------\n",result);
					return 0;
				}
			
				// Advance disk to next sector
				curDbuffPtr+= USLOSS_DISK_SECTOR_SIZE;
				curSector++;
				curSectorsToWrite--;
				
				pDebug(1," <- DiskDriver(): waitDevice pid[%d] After [Result:%d, Status:%d] DeviceOutput [Result:%d]...\n",temp->pid,result,status,resultD);
			}
			
			if(debugVal>1){
				printQ(DiskTable[unit].DriveQueueL,"Disk[%d] Left",unit);
				printQ(DiskTable[unit].DriveQueueR,"Disk[%d] Right",unit);
			}
			
			// TODO: Does the drive execute at DeviceOutput or waitDevice? If waitDevice pop here, otherwise pop uptop in stead of peek
			
			// Remove process just used for disk action from proper queue. 
			// If queue is empty, change drive direction (even though we are not using scan)
			if (DiskTable[temp->unit].drive_seek_dir == 0){
				pop(&DiskTable[temp->unit].DriveQueueL);
				if(DiskTable[temp->unit].DriveQueueL.count == 0)
					DiskTable[temp->unit].drive_seek_dir = 1;
			}else{
				pop(&DiskTable[temp->unit].DriveQueueR);
				if(DiskTable[temp->unit].DriveQueueR.count == 0)
					DiskTable[temp->unit].drive_seek_dir = 0;
			}
			
			if (DiskTable[temp->unit].DriveQueueR.count == 0 && DiskTable[temp->unit].DriveQueueL.count == 0){
			pDebug(1," <- DiskDriver(): Both QUEUES are EMPTY\n");
			}
		
			
			// Unblock Calling Process
			pDebug(1," <- DiskDriver(): unblocking calling process [%d] that requested disk[%d.%d] track[%d] firstSector[%d] sectors[%d]...\n",temp->pid,temp->unit,unit,temp->track,temp->first,temp->sectors);
			semvReal(temp->semID);			
		}
	}
	pDebug(2," <- DiskDriver(): end \n");
	return status;
}

static int TermDriver(char *arg){
    pDebug(2," <- TermDriver(): start \n");
    int result = 0;
	int resultD = 0;
    int status = 0;
	int unit = atoi(arg);
	char charReceived;
	int index = 0;
	int control = 0;
	
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
	enableInterrupts();
	
	// Initialize Term Unit from arg
	TermTable[unit].pid = getpid();
	sprintf(TermTable[unit].type,"Terminal %d",unit);
	TermTable[unit].semID = semcreateReal(0);
	TermTable[unit].mboxID = MboxCreate(0,0);
	
	// Let TermReader know there is a terminal to read
	control = USLOSS_TERM_CTRL_RECV_INT(control);
	resultD = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control);
	
    // Infinite loop until we are zap'd
    while(! isZapped()) {
		charReceived='_';
		result = waitDevice(USLOSS_TERM_DEV, unit, &control);
		
		if(debugVal>2)
			print_status(control);

		// if result is not 0 process probly zapped
		if (result != 0) {
			pDebug(1," <- TermDriver(): ------TERM[%d] QUIT WITH STATUS [%d]------\n",unit,result);
			return 0;
		}
		
		//TODO: I think we received something
		if(USLOSS_TERM_STAT_RECV(control) == USLOSS_DEV_BUSY){
			// Char has been received
			char charReceived = USLOSS_TERM_STAT_CHAR(control);
			TermReadTable[unit].receiveChar = charReceived;
			pDebug(2," <- TermDriver(): Term[%d] received char[%c], waking up TermWriter[%d]------\n",unit,charReceived,unit);
			
			// Wake up TermReader
			semvReal(TermReadTable[unit].semID);
		}
		if(USLOSS_TERM_STAT_XMIT(control) == USLOSS_DEV_READY){
			if(TermWriteTable[unit].requestQueue.count >0){
				pDebug(1," <- TermDriver(): Term[%d] ready to write, waking up TermWriter[%d]------\n",unit,unit);
				
				// Char has been XMIT send to TermWriter
				semvReal(TermWriteTable[unit].semID); //temp->pid
			}
		}
	}
	pDebug(2," <- TermDriver(): end \n");
	return status;
}

static int TermReader(char *arg){
    pDebug(2," <- TermReader(): start \n");
    int status = 0;
	int unit = atoi(arg);
	procPtr temp = NULL;
	int index=0;
	int resultD = 0;
	
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
	enableInterrupts();
	
	// Initialize TermReader Unit from arg
	TermReadTable[unit].pid = getpid();
	sprintf(TermReadTable[unit].type,"Reader %d",unit);
	TermReadTable[unit].semID = semcreateReal(0);
	TermReadTable[unit].mboxID = MboxCreate(0,0);
    
	// Infinite loop until we are zap'd
    while(! isZapped()) {
		pDebug(2," <- TermReader(): Before block on TermReader Unit[%d] semID[%d]\n",unit,TermReadTable[unit].semID);
		
		sempReal(TermReadTable[unit].semID);
		if(isZapped())
			return 0;
			// Let TermReader know there is a terminal to read

		pDebug(2," <- TermReader(): After block on TermReader Unit[%d] semID[%d] requestCount[%d]\n",unit,TermReadTable[unit].semID,TermReadTable[unit].requestQueue.count);

		TermTable[unit].t_line_buff[TermTable[unit].lineNumber][index] = TermReadTable[unit].receiveChar;
		index++;
		pDebug(2," <- TermReader(): unit[%d]charReceive[%c] index[%d]\n",unit,TermReadTable[unit].receiveChar, index);
		
		if(TermTable[unit].lineNumber == MAX_LINE_BUFFER){
			// Wake up TermReader.
			pDebug(2,"\n <- TermReader(): Before wake TermReader[%d] on semID[%d]\n",unit,TermReadTable[unit].semID);
		}else if(TermReadTable[unit].receiveChar == '\n'){
			pDebug(1," <- TermReader(): Term[%d], Newline found. Line RECEIVED = %s",unit,TermTable[unit].t_line_buff[TermTable[unit].lineNumber]);
			TermTable[unit].lineNumber++;
			index=0;
		}
		
		
		if(TermReadTable[unit].requestQueue.count > 0 && peek(TermReadTable[unit].requestQueue)->t_Op == USLOSS_TERM_READ){
			if(index == peek(TermReadTable[unit].requestQueue)->t_buff_size || TermReadTable[unit].receiveChar == '\n'){
				temp = pop(&TermReadTable[unit].requestQueue);
				
				pDebug(2,"\n <- TermReader(): Before TermReader[%d] attempts to read[%d] characters\n",unit,temp->t_buff_size);
				
				// temp variables
				char *tempBuff = temp->t_buff;
				
				pDebug(2," <- TermReader(): i==[%d] tempBuff[%p]\n",i,tempBuff);
				
				// Lets TermDriver know a request is coming
				push(&TermTable[unit].requestQueue,(long long)time(NULL),temp);
				
				// two cases: 
				
				//1) TermReader has buffered 10 lines and is blocked.
				//2) TermReader has finished reading before 10 and is not blocked, sitting on waitDevice.
				memcpy(tempBuff,TermTable[unit].t_line_buff[0],temp->t_buff_size);
				for(i = 1; i< TermTable[unit].lineNumber;i++){
					memcpy(TermTable[unit].t_line_buff[i-1],TermTable[unit].t_line_buff[i],MAXLINE);
				}
				bzero(TermTable[unit].t_line_buff[TermTable[unit].lineNumber], MAXLINE);
				TermTable[unit].lineNumber--;
				if(TermTable[unit].lineNumber == MAX_LINE_BUFFER-1)
					semvReal(TermTable[unit].semID);
				
				// Terminate string depending on line ended or requested less characters than a line.
				tempBuff[temp->t_buff_size] = '\0';
				pDebug(1," <- TermReader(): t_buff_size = [%d] strlen(tempBuff) = [%d] buffMsg = [%s]\n",temp->t_buff_size,strlen(tempBuff),tempBuff);
				temp->t_buff_size = strlen(tempBuff);
				
				// Unblock Calling Process
				pDebug(1," <- TermReader(): unblocking calling process [%d] that requested term[%d] size[%d] buffer[%p]...\n",temp->pid,temp->t_unit,temp->t_buff_size,temp->t_buff);
				semvReal(temp->semID);
			}
		}
	}
	
	pDebug(2," <- TermRead(): end \n");
	return status;
}

static int TermWriter(char *arg){
    pDebug(2," <- TermWriter(): start \n");
    int result = 0;
    int status = 0;
	int resultD = 0;
	int control = 0;
	int unit = atoi(arg);
	
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
	enableInterrupts();
	
	// Initialize TermRead Unit from arg
	TermWriteTable[unit].pid = getpid();
	sprintf(TermWriteTable[unit].type,"Writer %d",unit);
	TermWriteTable[unit].semID = semcreateReal(0);
	TermWriteTable[unit].mboxID = MboxCreate(0,0);
	
			// Block TermWriter waiting for input
		pDebug(1," <- TermWriter(): Before block on TermWriter Unit[%d] \n",unit);
		sempReal(TermWriteTable[unit].semID);
		
    // Infinite loop until we are zap'd
    while(! isZapped()) {
		

		
		// If zapped while blocked return
		if (isZapped()) {
			return 0;
		}
			
		pDebug(1," <- TermWriter(): After block on TermWriter Unit[%d] semID[%d] requestCount[%d]\n",unit,TermWriteTable[unit].semID,TermWriteTable[unit].requestQueue.count);
			
		// Inform USLOSS we are going to write
		control = 0;
		control = USLOSS_TERM_CTRL_XMIT_INT((long)control);
		resultD = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control);
		procPtr temp = peek(TermWriteTable[unit].requestQueue);
			
		if(TermWriteTable[unit].requestQueue.count > 0 && peek(TermWriteTable[unit].requestQueue)->t_Op == USLOSS_TERM_WRITE){
			

			int bytesWritten = 0;
			char* writeBuffer = temp->t_buff;
			
			// For each character requested to write, block and wait for TermDriver to tell us RDY to write.
			while(bytesWritten < temp->t_buff_size){
				sempReal(TermWriteTable[unit].semID);
				pDebug(1," <- TermWriter(): Writing char[%c]\n",writeBuffer[bytesWritten]);//TermWriteTable[unit].receiveChar
				control = 1;
				control =  USLOSS_TERM_CTRL_CHAR(control, writeBuffer[bytesWritten]);
				control = USLOSS_TERM_CTRL_XMIT_INT(control);
				control =  USLOSS_TERM_CTRL_XMIT_CHAR(control);

				bytesWritten++;
				resultD = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control); 				
								
				//writeBuffer++;
			}
			
			temp->t_buff_size = bytesWritten;
			pop(&TermWriteTable[unit].requestQueue);
			
			//Turn XMIT off
			control = 0;
			control = USLOSS_TERM_CTRL_XMIT_INT(control);
			resultD = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control); 
			
			// Unblock Calling Process
			pDebug(1," <- TermWriter(): unblocking calling process [%d] that requested term[%d] size[%d] buffer[%p]...\n",temp->pid,temp->t_unit,temp->t_buff_size,temp->t_buff);
			semvReal(temp->semID);
			
			
			//result = waitDevice(USLOSS_TERM_DEV, unit, &status);

		}
	}
	
	pDebug(2," <- TermWriter(): end \n");
	return status;
}

static int ClockDriver(char *arg){
	pDebug(2," <- ClockDriver(): start \n");
    int result;
    int status;
	int time;
	
    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
	enableInterrupts();
	
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

		pDebug(3," <- ClockDriver(): calling gettimeofdayReal\n"); 
		gettimeofdayReal(&time); 
		while(SleepList.count>0 && ((peek(SleepList)->sleepWakeAt)) <= time){
			procPtr temp = pop(&SleepList);
			semvReal(temp->semID);
			pDebug(2," <- ClockDriver(): waking up = pid[%d]\n",temp->pid); 
			
		}

	}
	pDebug(2," <- ClockDriver(): end \n");
	return status;
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
	pDebug(2," <- Sleep(): start \n");
    USLOSS_Sysargs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *)(long) seconds;
    USLOSS_Syscall((void *)(long) &sysArg);
    return (long) sysArg.arg4;
} /* end of Sleep */

void sleep(USLOSS_Sysargs *args){
	pDebug(2," <- sleep(): start \n");
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
 *				  int	sectors -- number of sectors to read
 *				  int   unit   -- unit number of the disk
 *                int   *status    -- pointer to output value
 *                (output value: completion status)
 *  Return Value: 0 means success, -1 means error occurs
 *
******************************************************************************/
int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status){
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *)(long) sectors;
    sysArg.arg3 = (void *)(long) track;
    sysArg.arg4 = (void *)(long) first;
    sysArg.arg5 = (void *)(long) unit;
    USLOSS_Syscall((void *)(long) &sysArg);
    *status = (long) sysArg.arg4;
    return (long) sysArg.arg4;
} /* end of DiskRead */

void diskRead(USLOSS_Sysargs *args){
	pDebug(2," <- diskRead(): start \n");
	int returnVal = diskReadReal(args->arg1,(long)args->arg3,(long)args->arg4, (long)args->arg2, (long)args->arg5);
	args->arg4 = (void*)(long)returnVal;
	args->arg1 = (void*)ProcTable[getpid()%MAXPROC].dbuff;
}

int diskReadReal(void *dbuff, int track, int first, int sectors, int unit){
	pDebug(1,"\n <- diskReadReal(): calling pid[%d] for unit[%d] track[%d] firstSector[%d] sectors[%d]\n",getpid()%MAXPROC,unit,track,first,sectors);
	
	// Check if arguments are bad
	if(unit <0 || unit >1 || track > DiskTable[unit].disk_track_size || track < 0 || first < 0 || first > DiskTable[unit].disk_track_size || (track == DiskTable[unit].disk_track_size && first +sectors > DiskTable[unit].disk_track_size))
		return -1;
	
	// Get calling process in ProcTable
	ProcTable[getpid()%MAXPROC].pid = getpid();
	ProcTable[getpid()%MAXPROC].diskOp = USLOSS_DISK_READ;
	ProcTable[getpid()%MAXPROC].dbuff = dbuff;
	ProcTable[getpid()%MAXPROC].track = track;
	ProcTable[getpid()%MAXPROC].first = first;
	ProcTable[getpid()%MAXPROC].sectors = sectors;
	ProcTable[getpid()%MAXPROC].unit = unit;

	// Push Request on Drive Queue
	if(DiskTable[unit].drive_seek_dir == 1){
		pDebug(1," <- diskWriteReal(): Seeking RIGHT...Pushing Track[%d], current Track[%d]", track,DiskTable[unit].DriveQueueR.count, DiskTable[unit].currentTrack);
		if (track >= DiskTable[unit].currentTrack){
			pDebug(1," disk can seek now.\n");
			push(&DiskTable[unit].DriveQueueR,track+1,&ProcTable[getpid()%MAXPROC]);
		}else{
			pDebug(1," disk track added alternate queue.\n");
			push(&DiskTable[unit].DriveQueueL,track+1,&ProcTable[getpid()%MAXPROC]);
		}
	}else{
		pDebug(1," <- diskWriteReal(): Seeking LEFT...Pushing Track[%d], current Track[%d]", track,DiskTable[unit].DriveQueueR.count, DiskTable[unit].currentTrack);
		if (track <= DiskTable[unit].currentTrack){
			pDebug(1," disk can seek now.\n");
			push(&DiskTable[unit].DriveQueueL,track+1,&ProcTable[getpid()%MAXPROC]);
		}else{
			pDebug(1," disk track added alternate queue.\n");
			push(&DiskTable[unit].DriveQueueR,track+1,&ProcTable[getpid()%MAXPROC]);
		}
	}
	
	// Wake up disk.
	semvReal(DiskTable[unit].semID);
	
	// Block Calling Process.
	sempReal(ProcTable[getpid()%MAXPROC].semID);
	
	return 0;
}

/******************************************************************************
 *  Routine:  DiskWrite
 *  Description: This is the call entry point for disk output.
 *  Arguments:    void* dbuff  -- pointer to the output buffer
 *                int   track  -- first track to write
 *                int   first -- first sector to write
 *				  int	sectors -- number of sectors to write
 *				  int   unit   -- unit number of the disk
 *                int   *status    -- pointer to output value
 *                (output value: completion status)
 *  Return Value: 0 means success, -1 means error occurs
******************************************************************************/
int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status){
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *)(long) sectors;
    sysArg.arg3 = (void *)(long) track;
    sysArg.arg4 = (void *)(long) first;
    sysArg.arg5 = (void *)(long) unit;
    USLOSS_Syscall((void *) &sysArg);
    *status = (long) sysArg.arg4;
    return (long) sysArg.arg4;
} /* end of DiskWrite */

void diskWrite(USLOSS_Sysargs *args){
	pDebug(2," <- diskWrite(): start\n");
	int returnVal = diskWriteReal(args->arg1,(long)args->arg3,(long)args->arg4, (long)args->arg2, (long)args->arg5);
	args->arg4 = (void*)(long)returnVal;
	args->arg1 = (void*)ProcTable[getpid()%MAXPROC].dbuff;
}

int diskWriteReal(void *dbuff, int track, int first, int sectors, int unit){
	pDebug(1," <- diskWriteReal(): calling pid[%d] for unit[%d] track[%d] firstSector[%d] sectors[%d]\n",getpid()%MAXPROC,unit,track,first,sectors);
	
	// Check if arguments are bad
	if(unit <0 || unit >1 || track > DiskTable[unit].disk_track_size || track < 0 || first < 0 || first > DiskTable[unit].disk_track_size || (track == DiskTable[unit].disk_track_size && first +sectors > DiskTable[unit].disk_track_size))
		return -1;
	
	// Get calling process in ProcTable
	ProcTable[getpid()%MAXPROC].pid = getpid();
	ProcTable[getpid()%MAXPROC].diskOp = USLOSS_DISK_WRITE;
	ProcTable[getpid()%MAXPROC].dbuff = dbuff;
	ProcTable[getpid()%MAXPROC].track = track;
	ProcTable[getpid()%MAXPROC].first = first;
	ProcTable[getpid()%MAXPROC].sectors = sectors;
	ProcTable[getpid()%MAXPROC].unit = unit;

	// Push Request on Drive Queue
	pDebug(1," <- diskWriteReal(): Requesting Track[%d] Sector[%d] Sectors[%d]. Current Track is [%d]", track,first,sectors, DiskTable[unit].currentTrack);
	if(DiskTable[unit].drive_seek_dir == 1){
		if (track >= DiskTable[unit].currentTrack){
			pDebug(1," disk can seek now.\n");
			push(&DiskTable[unit].DriveQueueR,track+1,&ProcTable[getpid()%MAXPROC]);
		}else{
			pDebug(1," disk can not seek backwards, add request to alternate queue.\n");
			push(&DiskTable[unit].DriveQueueL,track+1,&ProcTable[getpid()%MAXPROC]);
		}
	}else{
		if (track <= DiskTable[unit].currentTrack){
			pDebug(1," disk can seek now.\n");
			push(&DiskTable[unit].DriveQueueL,track+1,&ProcTable[getpid()%MAXPROC]);
		}else{
			pDebug(1," disk can not seek backwards, add request to alternate queue.\n");
			push(&DiskTable[unit].DriveQueueR,track+1,&ProcTable[getpid()%MAXPROC]);
		}
	}
	
		
	if(debugVal>2){
		printQ(DiskTable[unit].DriveQueueL,"Disk[%d] Left",unit);
		printQ(DiskTable[unit].DriveQueueR,"Disk[%d] Right",unit);
	}
	
	// Wake up disk.
	pDebug(1,"\n <- diskWriteReal(): Before wake disk[%d.%d]\n",unit,ProcTable[getpid()%MAXPROC].unit);
	semvReal(DiskTable[unit].semID);
	
	// Block Calling Process.
	pDebug(1," <- diskWriteReal(): Before Block calling pid[%d]\n",unit,ProcTable[getpid()%MAXPROC].unit,ProcTable[getpid()%MAXPROC].pid);
	sempReal(ProcTable[getpid()%MAXPROC].semID);
	pDebug(1," <- diskWriteReal(): After Block calling pid[%d]\n",ProcTable[getpid()%MAXPROC].pid);
	return 0;
}

/******************************************************************************
 *  Routine:  DiskSize
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
	pDebug(2," <- diskSize(): start \n");
	int unit = (long)args->arg1;
	int returnVal = diskSizeReal(unit);
	if(returnVal == 0){
		args->arg1 = (void*)(long)DiskTable[unit].disk_sector_size;
		args->arg2 = (void*)(long)DiskTable[unit].disk_track_size;
		args->arg3 = (void*)(long)DiskTable[unit].disk_size;
	}
	args->arg4 = (void*)(long)returnVal;
}
	
int diskSizeReal(int unit){
	pDebug(1," <- diskSizeReal(): start \n");
	if(unit <0 || unit >1)
		return -1;
	
	// Get calling process in ProcTable
	ProcTable[getpid()%MAXPROC].pid = getpid();
	ProcTable[getpid()%MAXPROC].diskOp = USLOSS_DISK_SIZE;

	
	// Push Request on Drive Queue
	push(&DiskTable[unit].DriveQueueR,(long long)time(NULL),&ProcTable[getpid()%MAXPROC]);
	
	// Wake up disk.
	semvReal(DiskTable[unit].semID);
	
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
	pDebug(2," <- termRead(): start \n");
	int returnVal = termReadReal((long)args->arg3,(long)args->arg2,args->arg1);
	args->arg2 = (void*)(long)ProcTable[getpid()%MAXPROC].t_buff_size;
	args->arg4 = (void*)(long)returnVal;
}

int termReadReal(int unit, int size, char *buff){
	int resultD = 0;	
	// Check if device call is valid range
	if(unit <0 || unit >USLOSS_TERM_UNITS || size < 1)
		return -1;
	
	// Get calling process in ProcTable
	ProcTable[getpid()%MAXPROC].pid = getpid();
	ProcTable[getpid()%MAXPROC].t_Op = USLOSS_TERM_READ;
	ProcTable[getpid()%MAXPROC].t_buff = buff;
	ProcTable[getpid()%MAXPROC].t_buff_size = size;
	ProcTable[getpid()%MAXPROC].t_unit = unit;

	
	pDebug(1," <- termReadReal(): pid[%d] pushing Request to TermWriter[%d] size[%d] buff[%p]\n",ProcTable[getpid()%MAXPROC].pid,unit,size,buff);
	
	push(&TermReadTable[unit].requestQueue,(long long)time(NULL),&ProcTable[getpid()%MAXPROC]);
	
	// Let TermReader know there is a terminal to read
	//control = USLOSS_TERM_CTRL_RECV_INT(control);
	//resultD = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*)(long)control);
	//pDebug(1," <- termReadReal(): DeviceOutput result[%d]\n",resultD);
	
	// Block Calling Process.
	pDebug(1," <- termReadReal(): Before Block calling pid[%d]\n",ProcTable[getpid()%MAXPROC].pid);
	sempReal(ProcTable[getpid()%MAXPROC].semID);
	pDebug(1," <- termReadReal(): After Block calling pid[%d]\n",ProcTable[getpid()%MAXPROC].pid);
	return 0;
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
	pDebug(2," <- termWrite(): start \n");
	int returnVal = termWriteReal((long)args->arg3,(long)args->arg2,args->arg1);
	args->arg2 = (void*)(long)ProcTable[getpid()%MAXPROC].t_buff_size;
	args->arg4 = (void*)(long)returnVal;
	
}

int termWriteReal(int unit, int size, char *buff){
	// Check if device call is valid range
	if(unit <0 || unit >USLOSS_TERM_UNITS || size < 1)
		return -1;
	
	// Get calling process in ProcTable
	ProcTable[getpid()%MAXPROC].pid = getpid();
	ProcTable[getpid()%MAXPROC].t_Op = USLOSS_TERM_WRITE;
	ProcTable[getpid()%MAXPROC].t_buff = buff;
	ProcTable[getpid()%MAXPROC].t_buff_size = size;
	ProcTable[getpid()%MAXPROC].t_unit = unit;

	
	//Push Request on Term Queue
	pDebug(1," <- termWriteReal(): pid[%d] pushing Request to TermWriter[%d] size[%d] buff[%p]\n",ProcTable[getpid()%MAXPROC].pid,unit,size,buff);
	push(&TermWriteTable[unit].requestQueue,(long long)time(NULL),&ProcTable[getpid()%MAXPROC]);
	
	// Unblock here unlike TermRead?
	semvReal(TermWriteTable[unit].semID);
	
	// Block Calling Process.
	pDebug(1," <- termWriteReal(): Before Block calling pid[%d]\n",ProcTable[getpid()%MAXPROC].pid);
	sempReal(ProcTable[getpid()%MAXPROC].semID);
	pDebug(1," <- termWriteReal(): After Block calling pid[%d]\n",ProcTable[getpid()%MAXPROC].pid);
	return 0;
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
		USLOSS_Console("Enable Inturrupts failed, halting...\n");
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
 *		     usloss console
 *****************************************************************************/
void dp4(){
    USLOSS_Console("\n------------------------------------------------PROCESS TABLE----------------------------------------------\n");
    USLOSS_Console(" PID  Name     Status S.At    S.Dur.    S.WakeAt    SemID MboxID diskOp dbuff      track first sectors unit\n");
    USLOSS_Console("-----------------------------------------------------------------------------------------------------------\n");
	for( i= 0; i< MAXPROC;i++){
		if(ProcTable[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %s[%-6s] %-0s[%d] %-2s[%-2d] %-3s[%-2d] %-5s[%-2d] %-7s[%-2d] %-1s[%-2d] %-2s[%-2d] %-1s[%-2d] %-1s[%-2d] %-1s[%-2d] %-1s[%-2d] %-3s[%-2d]\n","",ProcTable[i].pid,"", ProcTable[i].name,"",ProcTable[i].status,"",ProcTable[i].sleepAt,"",ProcTable[i].sleepDuration,"",ProcTable[i].sleepWakeAt,"",ProcTable[i].semID,"",ProcTable[i].mboxID,"",ProcTable[i].diskOp,"",&ProcTable[i].dbuff,"",ProcTable[i].track,"",ProcTable[i].first,"",ProcTable[i].sectors,"",ProcTable[i].unit);
		}	  
	}
    USLOSS_Console("-----------------------------------------------------------------------------------------------------------\n");    
}

/*****************************************************************************
 *     dd4 - Used to output a formatted version of the disk table to the 
 *		     usloss console
 *****************************************************************************/
void dd4(){
    USLOSS_Console("\n------------------------------------------------DISK TABLE----------------------------------------------\n");
    USLOSS_Console(" PID   c.Op   c.Track   c.Sector   semID   mboxID   QueueR   QueueL\n");
    USLOSS_Console("-----------------------------------------------------------------------------------------------------------\n");
	for( i= 0; i< USLOSS_DISK_UNITS;i++){
		if(DiskTable[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %-1s[%-2d] %-2s[%-2d] %-5s[%-2d] %-6s[%-2d] %-3s[%-2d] %-3s[%-2d] %-4s[%-2d]\n","",DiskTable[i].pid,"", DiskTable[i].currentOp,"",DiskTable[i].currentTrack,"",DiskTable[i].currentSector,"",DiskTable[i].semID,"",DiskTable[i].mboxID,"",DiskTable[i].DriveQueueR.count,"",DiskTable[i].DriveQueueL.count);
		}	  
	}
    USLOSS_Console("-----------------------------------------------------------------------------------------------------------\n");    
}

/*****************************************************************************
 *     dt4 - Used to output a formatted version of the term table to the 
 *		     usloss console
 *****************************************************************************/
void dt4(){
    USLOSS_Console("\n------------------------------------------------TERM TABLE----------------------------------------------\n");
    USLOSS_Console(" PID   c.Op   semID   mboxID   QueueCount   Type\n");
    USLOSS_Console("-----------------------------------------------------------------------------------------------------------\n");
	for( i= 0; i< USLOSS_TERM_UNITS;i++){
		if(TermTable[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %-1s[%-2d] %-2s[%-2d] %-3s[%-2d] %-3s[%-2d] %-8s[%-10s]\n","",TermTable[i].pid,"", TermTable[i].currentOp,"",TermTable[i].semID,"",TermTable[i].mboxID,"",TermTable[i].requestQueue.count,"",TermTable[i].type);
		}
		if(TermReadTable[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %-1s[%-2d] %-2s[%-2d] %-3s[%-2d] %-3s[%-2d] %-8s[%-10s]\n","",TermReadTable[i].pid,"", TermReadTable[i].currentOp,"",TermReadTable[i].semID,"",TermReadTable[i].mboxID,"",TermReadTable[i].requestQueue.count,"",TermReadTable[i].type);
		}
		if(TermWriteTable[i].pid != -1){ // Need to make legit determination for printing process
			USLOSS_Console("%-1s[%-2d] %-1s[%-2d] %-2s[%-2d] %-3s[%-2d] %-3s[%-2d] %-8s[%-10s]\n","",TermWriteTable[i].pid,"", TermWriteTable[i].currentOp,"",TermWriteTable[i].semID,"",TermWriteTable[i].mboxID,"",TermWriteTable[i].requestQueue.count,"",TermWriteTable[i].type);
		}		
	}
    USLOSS_Console("-----------------------------------------------------------------------------------------------------------\n");    
}

char* getOp(int op){
	return diskOps[op];
}