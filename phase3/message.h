#ifndef _MESSAGE_H
#define _MESSAGE_H


#include "phase2.h"
#include "MinQueue.h"
typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
typedef struct mprocStruct mprocStruct;
typedef struct mprocStruct *mprocPtr;
extern mprocStruct mprocStructE;
//other items as needed....

struct mailbox {
    int       mboxID;
    // other items as needed...
    int         slots;
    int         slot_size;
    int         currProc;
    int         slots_used;
    int         type; // Is it I/O mailbox?
    slotPtr     slotPtr;
    mboxProcPtr procPtr;
    MinQueue    receiveBlockList;
    MinQueue    sendBlockList;
    MinQueue    ReadyList;
};

struct mailSlot {
    int       mboxID;
    int       status;
    int       pid;
    // other items as needed...
    int       msg_length;
    char      message[MAX_MESSAGE];
    slotPtr   nextSlotPtr;
};

struct mprocStruct{
    int      pid;
    int      mboxID;
    int      msg_length;
    slotPtr   slotPtr;
    void     *msgPtr;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};

#define STATUS_NONE -1
#define STATUS_ACTIVE 1
#define STATUS_DEACTIVE 2
#define STANDARD_MAILBOX 10
#define DEBUG2 1

#endif