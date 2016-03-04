#ifndef _QUEUE_H
#define _QUEUE_H

typedef struct node {
	struct node *next;
	void *data;
} Node;

typedef struct queue {
    struct node *front;
    int size;
} Queue;

Queue* createQueue();
void enqueue(Queue *queue, void *node);
Node* dequeue(Queue *queue);
int size(Queue *queue);
void printQueue(Queue *queue, void (*callback)(void*));
void destroyQueue(Queue *queue);

Node* createNode(void *data);

#endif /* _QUEUE_H */