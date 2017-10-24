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
//void (*systemCallVec[MAXSYSCALLS])(systemArgs *args);
int 	mainSemaphore;

static int	ClockDriver(char *);
static int	DiskDriver(char *);

void start3(void){
    char	name[128];
    char    termbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;
	char 	buf[100];
	termbuf[0] = '\0';
	printf("%c",termbuf[0]);
    /*
     * Check kernel mode here.
     */
	check_kernel_mode("start3");
	
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
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
    }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver

    // eventually, at the end:
    quit(0);
    
}


static int ClockDriver(char *arg){
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(mainSemaphore);
    int enableInturruptResult = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
	if (enableInturruptResult == 0)
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
    }
	return status;
}

/*************************************************************************
*
*                           Phase 4 Entry Point
*
*************************************************************************/
//int start4(char * name){
//	return -1;
//}


/*************************************************************************
*
*                           System Calls
*
*************************************************************************/
int Sleep(int seconds){
	return -1;
}

int DiskRead (void *diskBuffer, int unit, int track, int first, int sectors, int *status){
	return -1;
}

int DiskWrite(void *diskBuffer, int unit, int track, int first, int sectors, int *status){
	return -1;
}

int DiskSize (int unit, int *sector, int *track, int *disk){
	return -1;
}

int TermRead (char *buffer, int bufferSize, int unitID, int *numCharsRead){
	return -1;
}

int TermWrite(char *buffer, int bufferSize, int unitID, int *numCharsRead){
	return -1;
}


/*************************************************************************
*
*                           DRIVERS
*
*************************************************************************/
static int DiskDriver(char *arg)
{
    return 0;
}

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
