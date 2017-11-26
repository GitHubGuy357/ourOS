PREFIX = ${HOME}

TARGET = libphase5.a
ASSIGNMENT = 452phase5
CC = gcc
AR = ar

COBJS = phase5.o p1.o libuser5.o
CSRCS = ${COBJS:.o=.c}

PHASE1LIB = patrickphase1
PHASE2LIB = patrickphase2
PHASE3LIB = patrickphase3
PHASE4LIB = patrickphase4
#PHASE1LIB = patrickphase1debug
#PHASE2LIB = patrickphase2debug
#PHASE3LIB = patrickphase3debug
#PHASE4LIB = patrickphase4debug

HDRS = vm.h

INCLUDE = ${PREFIX}/include

CFLAGS = -Wall -g -std=gnu99 -I. -I${INCLUDE} -DPHASE_3

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
	CFLAGS += -D_XOPEN_SOURCE
endif

LDFLAGS += -L. -L${PREFIX}/lib

TESTDIR = testcases
TESTS = test1 test2 test3 test4 simple1 simple2 simple3 simple4 simple5 simple6 \
	simple7 simple8 simple9 simple10 \
	chaos replace1 outOfSwap replace2 gen clock quit 
LIBS = -lusloss3.6 -l$(PHASE1LIB) -l$(PHASE2LIB) -l$(PHASE3LIB) \
       -lphase5 -l$(PHASE4LIB)

$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS) 

#$(TESTS):	$(TARGET) $(TESTDIR)/$$@.c
$(TESTS):	$(TARGET)
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS)

clean:
	rm -f $(COBJS) $(TARGET) test?.o test? simple?.o simple? simple??.o simple?? gen.o gen \
	chaos.o chaos quit.o quit replace?.o replace? outOfSwap.o \
	outOfSwap clock.o clock core term[0-3].out
	cp testcases/disk0.orig ./disk0
	cp testcases/disk1.orig ./disk1

submit: $(CSRCS) $(HDRS) $(TURNIN)
	tar cvzf phase5.tgz $(CSRCS) $(HDRS) Makefile
