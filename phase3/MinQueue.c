// Author: James Rogers (jarogers1)
// Date: 2017-10-10
// File: MinQueue.c
// CSC452 - OS


#include "MinQueue.h"
#include "phase2.h"
#include "phase3.h"
// Peek
struct procTable* peek(MinQueue head) {
	return head.topPtr->data;
}

/****************************************************************************
 * remove_data - Finds matching val_to_remove, if not found returns 0
 * otherwise removes
 ****************************************************************************/
int remove_data(MinQueue *heap, int val_to_remove) {
	Node *current = heap->topPtr;
	Node *previous = NULL;

	while (current != NULL && current->data->pid != val_to_remove) { //((procTable*)current->data)->mboxID
		previous = current;
		current = current->next;
	}
	if (current != NULL) {

		if (previous == NULL) {
			// Head being removed
			heap->topPtr = current->next;
		} else {
			previous->next = current->next;
		}
		heap->count--;
		return 1;
	} else
		return 0;
}

/****************************************************************************
 * print_queue - Prints the queue to stdio
 ****************************************************************************/
void printQ(MinQueue q) {
	printf("Process List: Count = [%d]\n",q.count);
	// WHY DOES THIS POINTER PRINTING DOES NOT WORK??? FIRST ELEMENT SHOWS IN DATA CORRECTLY, NEVER PRINTS, PRINTS 1 LESS ELEMENT AND THATS IT
	Node *tmp = q.topPtr;
	while (tmp != NULL) {
		printf(" ->Pid:[%d], Name:[%s] Status:[%d]\n", tmp->data->pid, tmp->data->name, tmp->data->status);
		tmp = tmp->next;
	}
}

/****************************************************************************
 * push - Pushes a value to the priority queue. If priority matches, it is added
 * to the end of that priority list.
 * INPUT: A min-heap.
 * OUTPUT: Returns the data held by priority, can be modified from struct procTable* to function for ready list.
 * SIDEEFFECTS: Alters the incoming heap.
 ****************************************************************************/
void push(MinQueue *heap, int priority, struct procTable* data) {
	if (heap->count != heap->size) {
		/* Build temp node */
		Node *tempPtr = getNextNode(heap);
		tempPtr->data = data;
		tempPtr->priority = priority;
		tempPtr->next = NULL;

		if (heap->count == 0) {
			/*Queue is empty add first val and set to topPtr*/
			heap->topPtr = tempPtr;
		} else {
			/*Queue is NOT empty, find position incoming priority goes*/
			Node *current = heap->topPtr;
			Node *previous = NULL;
			/*Find position for incoming priority */
			while (current != NULL && current->priority <= priority) {
				previous = current;
				current = current->next;
			}
			/* Add to queue in proper position */
			if (previous == NULL) {
				/* If priority is new low, add it as the top pointer */
				heap->topPtr = tempPtr;
				heap->topPtr->next = current;
			} else {
				/* If priority is between two others*/
				previous->next = tempPtr;
				tempPtr->next = current;
			}
		}
		heap->count += 1;
	} else
		printf("Queue full\n");
}

/****************************************************************************
 * push - Takes the min value node off the top of queue.
 * INPUT: A min-heap.
 * OUTPUT: Returns the data held by priority, can be modified from struct procTable* to function for ready list.
 * SIDEEFFECTS: Alters the incoming heap.
 ****************************************************************************/
struct procTable* pop(MinQueue *heap) {
	struct procTable* returnData = NULL;
	if (heap->count > 0) {
		Node *returnNode = heap->topPtr;
		returnData = returnNode->data;
		heap->topPtr = heap->topPtr->next;
		heap->count--;
	}
	return returnData;
}

/****************************************************************************
 * intialize_queue - Takes the min value node off the top of queue.
 * INPUT: A min-heap.
 * OUTPUT: None
 * SIDEEFFECTS: Alters the incoming heap initial values.
 ****************************************************************************/
void intialize_queue2(MinQueue *heap) {
	heap->size = MAXSIZE;
	heap->topPtr = NULL;
	heap->count = 0;
}

/****************************************************************************
 * getFirstNull - Finds the first non used node
 * INPUT: A min-heap.
 * OUTPUT: Node*, pointing to first availible node.
 * SIDEEFFECTS: None.
 ****************************************************************************/
Node* getNextNode(MinQueue *heap) {
	int i, found;

	// If topPtr is null, nothing is in queue, just reuturn first node.
	if (heap->topPtr == NULL)
		return &heap->nodes[0];

	// For each node in array, check to see if it exists in topPtr chain, if not, return it to be used.
	for (i = 0; i < MAXSIZE; i++) {
		Node *tempPtr = heap->topPtr;
		found = 0;
		while (tempPtr != NULL && !found) {
			if (tempPtr == &heap->nodes[i])
				found = 1;
			tempPtr = tempPtr->next;
		}
		if (!found)
			return &heap->nodes[i];
	}
	return NULL;
}
/****************************************************************************
 * MAIN - For testing
 ****************************************************************************/
 /*
int main() {
	MinQueue mHeap;
	intialize_queue2(&mHeap);
	int i, random;

	for (i = 0; i < mHeap.size; i++)
		printf("%d=mem[%p] ", i, &mHeap.nodes[i]);
	printf("\n\n");

// Breaks in test01 phase1
	 push(&mHeap, 6, "6");
	 push(&mHeap, 1, "1");
	 printf("%s\n",pop(&mHeap));
	 push(&mHeap, 3, "3");
	 push(&mHeap, 5, "5");
	 printf("%s\n",pop(&mHeap));
	 push(&mHeap, 1, "1");

// Breaks in test20 phase1
	push(&mHeap, 6, "6");
	push(&mHeap, 1, "1");
	printf("%s\n", pop(&mHeap));
	push(&mHeap, 2, "2");
	push(&mHeap, 3, "3");
	push(&mHeap, 4, "4");
	printf("%s\n", pop(&mHeap));
	push(&mHeap, 1, "1");
	printf("%s\n", pop(&mHeap));
	printf("%s\n", pop(&mHeap));
	push(&mHeap, 1, "1");
	printf("%s\n", pop(&mHeap));
	printf("%s\n", pop(&mHeap));

// Manual Push
	 push(&mHeap, 5, "5");
	 push(&mHeap, 6, "6");
	 push(&mHeap, 4, "4");
	 push(&mHeap, 3, "3");
	 push(&mHeap, 7, "7");
	 push(&mHeap, 2, "2");
	 push(&mHeap, 1, "1");
	 push(&mHeap, 1, "after1");

// Test remove
	 if (remove_data(&mHeap, "8"))
	 printf("remove_data(): Deleted\n");
	 else
	 printf("remove_data(): Not Found\n");
	 if (remove_data(&mHeap, "7"))
	 printf("remove_data(): Deleted\n");
	 else
	 printf("remove_data(): Not Found\n");
	 if (remove_data(&mHeap, "1"))
	 printf("remove_data(): Deleted\n");
	 else
	 printf("remove_data(): Not Found\n");
// Automated Push Test
//	for (i = 0; i < mHeap.size; i++) {
//		random = rand() % mHeap.size + 1;
//		printf("i=[%d] random=[%d]\n", i, random);
//		int priority = random;
//		struct char* data = malloc(5);
//		sprintf(data, "%d", priority);
//		push(&mHeap, priority, data);
//	}

// Pop the list
	 printf("\nPOPPING\n");
	 while (mHeap.count > 0) {
	 char* value = pop(&mHeap);
	 printf("%s\n", value);
	 i++;
	 }

	return 0;
}
*/
