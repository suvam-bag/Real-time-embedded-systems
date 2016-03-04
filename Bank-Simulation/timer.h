#ifndef _TIMER_H
#define _TIMER_H

#include <signal.h>

// Each timer is associated with a real-time signal
#define TIMER1 (SIGRTMIN+5)
#define TIMER2 (SIGRTMIN+6)
#define TIMER3 (SIGRTMIN+7)
#define TIMER4 (SIGRTMIN+8)
#define TIMER5 (SIGRTMIN+9)
#define TIMER6 (SIGRTMIN+10)
#define TIMER7 (SIGRTMIN+11)
#define TIMER8 (SIGRTMIN+12)


void setSignalToBlock(int);
void startTimer(int, int);
void waitForSignal(int);
void bindHandlerToSignal(int);
long getTimer(int);
void destroyTimer(int);
void signalHandler(int, siginfo_t*, void*);

#endif /* _TIMER_H */
