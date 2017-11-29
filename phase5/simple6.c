/*
 * simple6.c
 *
 * One process writes every page, where frames = pages-1. If the clock
 * algorithm starts with frame 0, this will cause a page fault on every
 * access.  This test requires writing pages to disk and reading them
 * back in the next time the page is accessed. The test checks for the
 * correct number of page faults, page ins, and page outs.
 */
#include <usloss.h>
#include <usyscall.h>
#include <phase5.h>
#include <libuser.h>
#include <string.h>
#include <assert.h>

#define Tconsole USLOSS_Console

#define TEST        "simple6"
#define PAGES       7
#define CHILDREN    1
#define FRAMES      (PAGES-1)
#define PRIORITY    5
#define ITERATIONS  10
#define PAGERS      1
#define MAPPINGS    PAGES

void *vmRegion;

int sem;

void test_setup(int argc, char *argv[])
{
}

void test_cleanup(int argc, char *argv[])
{
}

int
Child(char *arg)
{
    int      pid;
    int      page;
    int      i;
    //char     *buffer;
    VmStats  before;
    int      value;

    GetPID(&pid);
    Tconsole("\nChild(%d): starting\n", pid);

    //buffer = (char *) vmRegion;

    for (i = 0; i < ITERATIONS; i++) {
        Tconsole("\nChild(%d): iteration %d\n", pid, i);
        before = vmStats;
        for (page = 0; page < PAGES; page++) {
            Tconsole("Child(%d): writing to page %d\n", pid, page);
            // Write page as an int to the 1st 4 bytes of the page
            * ((int *) (vmRegion + (page * USLOSS_MmuPageSize()))) = page;
            // Write page as a letter to the 5th byte of the page
            * ((char *) (vmRegion + (page * USLOSS_MmuPageSize()) + 4)) =
                'A' + page;
            value = * ((int *) (vmRegion + (page * USLOSS_MmuPageSize())));
            assert(value == page);
        }
        assert(vmStats.faults - before.faults == PAGES);
    }
    assert(vmStats.faults == PAGES * ITERATIONS);
    assert(vmStats.new == PAGES);
    assert(vmStats.pageIns == PAGES * (ITERATIONS - 1));
    assert(vmStats.pageOuts == PAGES * ITERATIONS - FRAMES);

    SemV(sem);

    Tconsole("\n");

    Terminate(127);
    return 0;
} /* Child */


int
start5(char *arg)
{
    int  pid;
    int  status;

    Tconsole("start5(): Running:    %s\n", TEST);
    Tconsole("start5(): Pagers:     %d\n", PAGERS);
    Tconsole("          Mappings:   %d\n", MAPPINGS);
    Tconsole("          Pages:      %d\n", PAGES);
    Tconsole("          Frames:     %d\n", FRAMES);
    Tconsole("          Children:   %d\n", CHILDREN);
    Tconsole("          Iterations: %d\n", ITERATIONS);
    Tconsole("          Priority:   %d\n", PRIORITY);

    status = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS, &vmRegion );

    Spawn("Child", Child,  0,USLOSS_MIN_STACK*7,PRIORITY, &pid);
    SemP( sem);
    Wait(&pid, &status);
    assert(status == 127);

    Tconsole("start5(): done\n");
    //PrintStats();
    VmDestroy();
    Terminate(1);

    return 0;
} /* start5 */
