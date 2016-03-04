#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

Node* createNode(void *data) {
	Node *node = (Node*)malloc(sizeof(Node));
	node->next = NULL;
	node->data = data;
	return node;
}

void enqueue(Queue *queue, void *node) {
	if(queue->front == NULL) {
		queue->front = node;
    } else {
    	Node *curr = queue->front;
    	while(curr->next != NULL) {
    		curr = curr->next;
    	}
    	curr->next = node;
    }
    queue->size++;
}

Node* dequeue(Queue *queue) {
	if(queue->front != NULL) {
		Node *node = queue->front;
		queue->front = queue->front->next;
		queue->size--;
		return node;
	}
	return NULL;
}

int size(Queue *queue) {
	return queue->size;
}

void printQueue(Queue *queue, void (*callback)(void*)) {
	Node *curr = queue->front;
	while(curr != NULL) {
		callback(curr->data);
		curr = curr->next;
	}
	printf("\n");
}

Queue* createQueue() {
	Queue *queue = (Queue*)malloc(sizeof(Queue));
	queue->front = NULL;
	queue->size = 0;
	return queue;
}

void destroyQueue(Queue *queue) {
	Node *curr = queue->front;
	while(curr != NULL) {
		Node *temp = curr;
		curr = curr->next;
		free(temp);
	}
	free(queue);
}