#ifndef _MINQUEUE_H
#define _MINQUEUE_H
#define MAXSIZE 100
#include <stdio.h>
#include <stdlib.h>

//The node
typedef struct Node {
	long long priority;
	struct Node * next;
	struct mprocStruct * data;
} Node;

//The heap
 typedef struct MinQueue {
	struct Node nodes[MAXSIZE];
	int size;
	int count;
	Node * topPtr;
} MinQueue;

//Prototypes
struct mprocStruct* peek(MinQueue head);
int remove_data(MinQueue *heap, int val_to_remove);
void print(MinQueue * q);
void push(MinQueue *heap, int priority, struct mprocStruct* data);
struct mprocStruct* pop(MinQueue *heap);
void intialize_queue2(MinQueue *heap);

#else

#endif