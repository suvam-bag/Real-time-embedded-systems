#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "timer.h"

#define MAX_SIGNALS 64

timer_t timers[MAX_SIGNALS]; // Array of timers

void setSignalToBlock(int signo) {
    sigset_t sigs_to_block;
    sigemptyset(&sigs_to_block);
    sigaddset(&sigs_to_block, signo); 
    pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);
}

void startTimer(int signo, int ms) {
    static struct sigevent sigev;
    static timer_t tid;
    static struct itimerspec itval;
    static struct itimerspec oitval;

    bindHandlerToSignal(signo);

    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = signo;
    sigev.sigev_value.sival_ptr = &tid;
    
    timer_create(CLOCK_REALTIME, &sigev, &tid);
    timers[signo] = tid;
    itval.it_value.tv_sec = ms / 1000;
    itval.it_value.tv_nsec = (long)(ms % 1000) * (1000000L);
    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = 0;
    timer_settime(tid, 0, &itval, &oitval);
}

void waitForSignal(int signo) {
    sigset_t sigs_to_catch; 
    int caught; 
    sigemptyset(&sigs_to_catch);    
    sigaddset(&sigs_to_catch, signo);
    sigwait(&sigs_to_catch, &caught);
}

void bindHandlerToSignal(int signo) {
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = signalHandler;
    sigaction(signo, &sigact, NULL);
}

long getTimer(int signo) {
    struct itimerspec timer;
    timer_gettime(timers[signo], &timer);
    return timer.it_value.tv_sec;
}

void destroyTimer(int signo) {
    timer_delete(timers[signo]);
}

void signalHandler(int signo, siginfo_t* info, void* context) {} // Dummy signal handler
