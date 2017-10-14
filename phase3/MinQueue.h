#ifndef _MINQUEUE_H
#define _MINQUEUE_H
#define MAXSIZE 100
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//The node
typedef struct Node {
	long long priority;
	struct Node * next;
	struct procTable * data;
} Node;

//The heap
 typedef struct MinQueue {
	struct Node nodes[MAXSIZE];
	int size;
	int count;
	Node * topPtr;
} MinQueue;

//Prototypes
struct procTable* peek(MinQueue head);
int remove_data(MinQueue *heap, int val_to_remove);
void printQ(MinQueue q);
void push(MinQueue *heap, int priority, struct procTable* data);
struct procTable* pop(MinQueue *heap);
void intialize_queue2(MinQueue *heap);

#else

#endif