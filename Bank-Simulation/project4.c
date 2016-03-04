#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include "timer.h"
#include "queue.h"

#define MIN_CUSTOMER_ARRIVAL_TIME   100     // ms or 1 minute
#define MAX_CUSTOMER_ARRIVAL_TIME   400     // ms or 4 minutes

#define MIN_CUSTOMER_TELLER_TIME    50      // ms or 30 seconds
#define MAX_CUSTOMER_TELLER_TIME    600     // ms or 6 minutes

#define MIN_BREAK_TIME	100				//ms or 1 minute
#define MAX_BREAK_TIME	400				//ms or 4 minutes

#define DAY_END                     42000   // ms or 420 minutes

#define TIMER_CUSTOMER  TIMER1
#define TIMER_DAY       TIMER2
#define TIMER_TELLER0   TIMER3
#define TIMER_TELLER1   TIMER4
#define TIMER_TELLER2   TIMER5
#define TIMER_BREAK0	TIMER6
#define TIMER_BREAK1	TIMER7
#define TIMER_BREAK2	TIMER8

typedef struct customer {
    int id;
    int timeSpentWaitingInQueue;
    int timeSpentWithTeller;
} Customer;

typedef struct teller {
    int id;
    long timeSpentWaitingForCustomers;
    long timeSpentWaitingForCustomer;
    int timeSpentDoingTransaction;
    int numberOfCustomersServiced;
    int timeSpentDuringBreak;
} Teller;

Queue *queue;           // Queue of customers waiting to be serviced
Queue *queueFinished;   // Queue that customers go into after they are serviced

sem_t semaphore;        // To control access to the three teller resources
sem_t mutex;            // To prevent the queue from being accessed by more than one thread at a time

int id = 0;             // Customer id gets increment as customers get generated
int maxWaitTimeForTellersWaitingForCustomer = 0;
int maxCustomerWaitTimeInQueue = 0;
int maxTransactionTimeForTeller = 0;
int maxDepth = 0;


Customer* createCustomer(long timeEnteredQueue) {
    Customer *customer = (Customer*)malloc(sizeof(Customer));
    customer->id = id;
    id++;
    customer->timeSpentWaitingInQueue = timeEnteredQueue;
    customer->timeSpentWithTeller = 0;
    return customer;
}

void printCustomer(void *data) {
    printf("%d ", ((Customer*)data)->id);
}

void customerStartedWaitingInQueue() {
    enqueue(queue, createNode(createCustomer(getTimer(TIMER_DAY)))); // Add customer to the queue
    if (size(queue) > maxDepth)
        maxDepth = size(queue);
    printf("Queue: ");
    printQueue(queue, printCustomer);
}



void* customerScheduler(void *arg) {
    int r;
    while(getTimer(TIMER_DAY)) {
        r = rand() % ((MAX_CUSTOMER_ARRIVAL_TIME - MIN_CUSTOMER_ARRIVAL_TIME) + 1) + MIN_CUSTOMER_ARRIVAL_TIME;
        startTimer(TIMER_CUSTOMER, r);   // Start a timer with the random expiration
        waitForSignal(TIMER_CUSTOMER);   // Block until timer expires
        sem_wait(&mutex);                // Only one threaad should access queue at a time
        customerStartedWaitingInQueue();
        sem_post(&mutex);
    }
    return 0;
}

void tellerBreak(Teller *teller){
	int p, timer_break;

	printf("Teller %d now is on break\n", teller->id);
	p = rand() % ((MAX_BREAK_TIME - MIN_BREAK_TIME) +1) + MIN_BREAK_TIME;
	teller->timeSpentDuringBreak = p;



		switch(teller->id){
			case 0:
            	timer_break = TIMER_BREAK0;
            	break;
			case 1:
            	timer_break = TIMER_BREAK1;
            	break;
			case 2:
            	timer_break = TIMER_BREAK2;
            	break;
			default:
            	break;


		}
	startTimer(timer_break, p);
	waitForSignal(timer_break);
}

void serviceCustomer(Teller *teller, Customer *customer) {
    int r, timer;

    printf("Teller %d now servicing Customer %d\n", teller->id, customer->id);
    teller->numberOfCustomersServiced += 1;
    r = rand() % ((MAX_CUSTOMER_TELLER_TIME - MIN_CUSTOMER_TELLER_TIME) + 1) + MIN_CUSTOMER_TELLER_TIME;
    teller->timeSpentDoingTransaction = r;
    if (teller->timeSpentDoingTransaction > maxTransactionTimeForTeller)
        maxTransactionTimeForTeller = teller->timeSpentDoingTransaction;
    customer->timeSpentWithTeller = r;
    switch(teller->id) {
        case 0:
            timer = TIMER_TELLER0;
            break;
        case 1:
            timer = TIMER_TELLER1;
            break;
        case 2:
            timer = TIMER_TELLER2;
            break;
        default:
            break;
    }

    startTimer(timer, r);   // Start a timer with the random expiration
    waitForSignal(timer);   // Block until timer expires
}

void tellerStartWaitingForCustomer(Teller *teller, int *teller_not_started_waiting) {
    teller->timeSpentWaitingForCustomers += getTimer(TIMER_DAY);
    teller->timeSpentWaitingForCustomer = getTimer(TIMER_DAY);
    printf("Teller %d started waiting at %ld\n", teller->id, getTimer(TIMER_DAY));
    *teller_not_started_waiting = 0;
}

void tellerFinishedWaitingForCustomer(Teller *teller, int *teller_not_started_waiting) {
    teller->timeSpentWaitingForCustomers -= getTimer(TIMER_DAY);
    teller->timeSpentWaitingForCustomer -= getTimer(TIMER_DAY);
    if (teller->timeSpentWaitingForCustomer > maxWaitTimeForTellersWaitingForCustomer)
        maxWaitTimeForTellersWaitingForCustomer = teller->timeSpentWaitingForCustomer;
    printf("Teller %d finished waiting at %ld\n", teller->id, getTimer(TIMER_DAY));
    *teller_not_started_waiting = 1;
}

Customer* customerFinishedWaitingInQueue() {
    Customer *customer;
    Node* dq = dequeue(queue);      // Remove customer from queue. NULL if empty 
    if (dq) {
        customer = (Customer*)dq->data;
        customer->timeSpentWaitingInQueue -= getTimer(TIMER_DAY);
        if (customer->timeSpentWaitingInQueue > maxCustomerWaitTimeInQueue)
            maxCustomerWaitTimeInQueue = customer->timeSpentWaitingInQueue;
        enqueue(queueFinished, dq); // Add customer to finished queue
        return customer;
    }
    return NULL;
}

void* startTeller(void *arg) {
    Teller *teller = (Teller*) arg;
    Customer *customer;
    int teller_not_started_waiting = 1;

    printf("Teller %d started\n", teller->id);
    while (getTimer(TIMER_DAY) || size(queue)) {
        sem_wait(&semaphore); // Make a teller busy
        sem_wait(&mutex);     // Only one threaad should access queue at a time
        customer = customerFinishedWaitingInQueue();
        sem_post(&mutex);
        if (customer) {
            if (!teller_not_started_waiting) {
                tellerFinishedWaitingForCustomer(teller, &teller_not_started_waiting);
            }
            serviceCustomer(teller, customer);
            tellerBreak(teller);
        } else {
            if (teller_not_started_waiting) {
                tellerStartWaitingForCustomer(teller, &teller_not_started_waiting);
            }
        }
        sem_post(&semaphore);
    }
}

void initializeTeller(int id, Teller *teller) {
    teller->id = id;
    teller->timeSpentWaitingForCustomers = 0;
    teller->timeSpentWaitingForCustomer = 0;
    teller->timeSpentDoingTransaction = 0;
    teller->numberOfCustomersServiced = 0;
    teller->timeSpentDuringBreak = 0;
}

int calculateTotalNumberOfCustomersServicedDuringDay(void) {
    return size(queueFinished);
}

float calculateAverageTimeEachCustomerSpendsWaitingInQueue(void) {
    Node *curr = queueFinished->front;
    int total = 0;
    while(curr != NULL) {
        total += ((Customer*)curr->data)->timeSpentWaitingInQueue;
        curr = curr->next;
    }
    return total / (float) size(queueFinished);
}

float calculateAverageTimeEachCustomerSpendsWithTeller(void) {
    Node *curr = queueFinished->front;
    int total = 0;
    while(curr != NULL) {
        total += ((Customer*)curr->data)->timeSpentWithTeller;
        curr = curr->next;
    }
    return total / (float) size(queueFinished);
}

float calculateAverageTimeTellersWaitForCustomer(Teller *teller0, Teller *teller1, Teller *teller2) {
    int total = teller0->timeSpentWaitingForCustomers + 
                teller1->timeSpentWaitingForCustomers +
                teller2->timeSpentWaitingForCustomers;
    return (total / 3.0) / (float) size(queueFinished);
}

int calculateMaximumCustomerWaitTimeInQueue(void) {
    return maxCustomerWaitTimeInQueue;
}

int calculateMaximumWaitTimeForTellersWaitingForCustomer(Teller *teller0, Teller *teller1, Teller *teller2) {
    return maxWaitTimeForTellersWaitingForCustomer;
}

int calculateMaximumTransactionTimeForTeller(Teller *teller0, Teller *teller1, Teller *teller2) {
    return maxTransactionTimeForTeller;
}

int calculateMaximumDepthOfQueue(void) {
    return maxDepth;
}

float msToMins(float ms) {
    return ms / 100.0;
}

int main() {
    pthread_t teller0, teller1, teller2, custScheduler;
    Teller t0, t1, t2;

    initializeTeller(0, &t0);
    initializeTeller(1, &t1);
    initializeTeller(2, &t2);

    setSignalToBlock(TIMER_CUSTOMER);
    setSignalToBlock(TIMER_DAY);
    setSignalToBlock(TIMER_TELLER0);
    setSignalToBlock(TIMER_TELLER1);
    setSignalToBlock(TIMER_TELLER2);
    setSignalToBlock(TIMER_BREAK0);
    setSignalToBlock(TIMER_BREAK1);
    setSignalToBlock(TIMER_BREAK2);

    srand(time(NULL));
    
    queue = createQueue();
    queueFinished = createQueue();
    
    sem_init(&semaphore, 0, 3);
    sem_init(&mutex, 0, 1);

    startTimer(TIMER2, DAY_END);
    
    pthread_create(&teller0, NULL, startTeller, (void*) &t0);
    pthread_create(&teller1, NULL, startTeller, (void*) &t1);
    pthread_create(&teller2, NULL, startTeller, (void*) &t2);
    pthread_create(&custScheduler, NULL, customerScheduler, NULL);

    pthread_join(custScheduler, NULL); 
    pthread_join(teller0, NULL);
    pthread_join(teller1, NULL);
    pthread_join(teller2, NULL); 

    int   calc1 = calculateTotalNumberOfCustomersServicedDuringDay();
    float calc2 = calculateAverageTimeEachCustomerSpendsWaitingInQueue();
    float calc3 = calculateAverageTimeEachCustomerSpendsWithTeller();
    float calc4 = calculateAverageTimeTellersWaitForCustomer(&t0, &t1, &t2);
    int   calc5 = calculateMaximumCustomerWaitTimeInQueue();
    int   calc6 = calculateMaximumWaitTimeForTellersWaitingForCustomer(&t0, &t1, &t2);
    int   calc7 = calculateMaximumTransactionTimeForTeller(&t0, &t1, &t2);
    int   calc8 = calculateMaximumDepthOfQueue();


    printf("The total number of customers serviced during the day: %d\n", calc1);
    printf("The average time each customer spends waiting in the queue: %.2f ms or %.2f mins\n", calc2, msToMins(calc2));
    printf("The average time each customer spends with the teller: %.2f ms or %.2f mins\n", calc3, msToMins(calc3));
    printf("The average time tellers wait for customer: %.2f ms or %.2f mins\n", calc4, msToMins(calc4));
    printf("The maximum customer wait time in the queue: %d ms or %.2f mins\n", calc5, msToMins(calc5));
    printf("The maximum wait time for tellers waiting for customer: %d ms or %.2f mins\n", calc6, msToMins(calc6));
    printf("The maximum transaction time for a teller: %d ms or %.2f mins\n", calc7, msToMins(calc7));
    printf("The maximum depth of the queue: %d\n", calc8);


    destroyQueue(queue);
    destroyQueue(queueFinished);
    sem_destroy(&semaphore);
    sem_destroy(&mutex);

    return 0;
}
