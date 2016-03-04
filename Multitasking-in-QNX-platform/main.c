#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <hw/inout.h>     /* for in*() and out*() functions */
#include <sys/mman.h>     /* for mmap_device_io() */
#include <sys/neutrino.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <termios.h>
#include <sys/select.h>
#include <string.h>
#include <semaphore.h>

#define BASE        0x280
#define CONTROL     (BASE+11)
#define PORT_B      (BASE+9)
#define PORT_LENGTH 1 // byte

#define PORT_B_MODE 1
#define PWM0         3 // Pin 12
#define PWM1         4 // Pin 13

#define OUTPUT      0

#define LOW         0
#define HI          1

#define PERIOD      (20 * 1000)

#define STOP        0
#define POS0        200
#define POS1        600
#define POS2        1000
#define POS3        ((1 * 1000) + 400)
#define POS4        ((1 * 1000) + 800)
#define POS5        ((2 * 1000) + 200)

#define MOV        (0x1 << 5) // Parameter: 0-5
#define WAIT       (0x2 << 5) // Parameter: 0-31
#define LOOP       (0x4 << 5) // Parameter: 0-31
#define END_LOOP   (0x5 << 5)
#define RECIPE_END (0x0 << 5)

#define RECIPE0_LENGTH 7
const unsigned char recipe0[RECIPE0_LENGTH] = {
	MOV|5,
	MOV|4,
	MOV|3,
	MOV|2,
	MOV|1,
	MOV|0,
	RECIPE_END
};

#define RECIPE1_LENGTH 8
const unsigned char recipe1[RECIPE1_LENGTH] = {
	MOV|5,
	MOV|4,
	MOV|3,
	WAIT|31,
	MOV|2,
	MOV|1,
	MOV|0,
	RECIPE_END
};

#define RECIPE2_LENGTH 9
const unsigned char recipe2[RECIPE2_LENGTH] = {
	MOV|5,
	MOV|4,
	LOOP|3,
	MOV|3,
	MOV|2,
	END_LOOP,
	MOV|1,
	MOV|0,
	RECIPE_END
};

#define SELECTED_RECIPE_LENGTH RECIPE2_LENGTH
const unsigned char (*selected_recipe)[SELECTED_RECIPE_LENGTH] = &recipe2;

int pos[] = { POS0, POS1, POS2, POS3, POS4, POS5 };

typedef struct servo {
	int id;
} Servo;

char c = 'x';
char d = 'x';

pthread_mutex_t mutex_c = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_d = PTHREAD_MUTEX_INITIALIZER;

int si = 0;
int sj = 0;

int flag_pause_servo0 = 1;
int flag_pause_servo1 = 1;

int curr_pos_servo0 = 0;
int curr_pos_servo1 = 0;

int loop_servo0_start;
int loop_servo0_times;

int loop_servo1_start;
int loop_servo1_times;

struct termios orig_termios;

void p(int s,void* p){int
i,j;for(i=s-1;i>=0;i--)for(j=7;j>=0;j--)printf("%u",(*((unsigned
char*)p+i)&(1<<j))>>j);puts("");}

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode() {
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch() {
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

void getRootPermissions(void) {
    int privity_err;

    // Give this thread root permissions to access the hardware
    privity_err = ThreadCtl(_NTO_TCTL_IO, NULL);
    if (privity_err == -1) {
       fprintf( stdout, "can't get root permissions\n" );
    }
}

void setBitInRegister(int reg, int bit, int value) {
    uintptr_t handle;
    uint8_t byte;
    handle = mmap_device_io(PORT_LENGTH, reg);
    byte = in8(handle);
    if (value == 0)
        byte &= ~(1 << bit);
    else // value == 1
        byte |= (1 << bit);
    out8(handle, byte);
    munmap_device_io(handle, PORT_LENGTH);
}

void printRegister(int reg, char *name) {
    uintptr_t handle;
    uint8_t byte;
    handle = mmap_device_io(PORT_LENGTH, reg);
    byte = in8(handle);
    printf("%s: ", name);
    p(sizeof(byte), &byte);
    munmap_device_io(handle, PORT_LENGTH);
}

void position(int t_on, int pwm) {
    int i = 0;
    for(i = 0; i < 28; i++) {
        setBitInRegister(PORT_B, pwm, LOW);
        usleep((useconds_t)PERIOD - t_on);
        setBitInRegister(PORT_B, pwm, HI);
        usleep((useconds_t)t_on);
        setBitInRegister(PORT_B, pwm, LOW);
    }
}

void stop(int pwm) {
	setBitInRegister(PORT_B, pwm, LOW);
}

void execute(unsigned int command, int pwm) {
	unsigned int opcode;
	unsigned int parameter;

	opcode = command >> 5;
	parameter = command ^ (opcode << 5);

	switch (opcode << 5) {
		case MOV:
			position(pos[parameter], pwm);
			if(pwm == PWM0)
				curr_pos_servo0 = parameter;
			else if (pwm == PWM1)
				curr_pos_servo1 = parameter;
			break;
		case WAIT:
			usleep(100 * 1000 * parameter);
			break;
		case LOOP:
			if(pwm == PWM0) {
				loop_servo0_start = si;
				loop_servo0_times = parameter;
			}
			else if (pwm == PWM1) {
				loop_servo1_start = sj;
				loop_servo1_times = parameter;
			}
			break;
		case END_LOOP:
			if(pwm == PWM0) {
				if(loop_servo0_times != 0) {
					si = loop_servo0_start;
					loop_servo0_times--;
				}
			}
			else if (pwm == PWM1) {
				if(loop_servo1_times != 0) {
					sj = loop_servo1_start;
					loop_servo1_times--;
				}
			}
			break;
		case RECIPE_END:
			stop(pwm);
			break;
		default: break;
	}
}

void* startServo0(void *arg) {
	Servo *servo = (Servo*)arg;
	printf("%d\n", servo->id);
	for(;;) {
		pthread_mutex_lock(&mutex_c);
		if(c == 'p') {
			stop(PWM0);
			flag_pause_servo0 = 0;
			c = 'x';
		}
		if(c == 'c') {
			flag_pause_servo0 = 1;
			c = 'x';
		}
		if(c == 'b') {
			if((*selected_recipe)[si] == RECIPE_END) {
				si = 0;
			}
			c = 'x';
		}
		if(c == 'l') {
			if(flag_pause_servo0 == 0) {
				if(curr_pos_servo0 != 0) {
					printf("servo0 left from %d\n", curr_pos_servo0);
					position(pos[curr_pos_servo0-1], PWM0);
					curr_pos_servo0 -= 1;
				}
			}
			c = 'x';
		}
		if(c == 'r') {
			if(flag_pause_servo0 == 0) {
				if(curr_pos_servo0 != 5) {
					printf("servo0 right from %d\n", curr_pos_servo0);
					position(pos[curr_pos_servo0+1], PWM0);
					curr_pos_servo0 += 1;
				}
			}
			c = 'x';
		}
		pthread_mutex_unlock(&mutex_c);
		if(flag_pause_servo0) {
			execute((*selected_recipe)[si], PWM0);
			if(si+1 < SELECTED_RECIPE_LENGTH)
				si++;
			sleep(1);
		}
	}
	return 0;
}

void* startServo1(void *arg) {
	Servo *servo = (Servo*)arg;
	printf("%d\n", servo->id);
	for(;;) {
		pthread_mutex_lock(&mutex_d);
		if(d == 'p') {
			stop(PWM1);
			flag_pause_servo1 = 0;
			d = 'x';
		}
		if(d == 'c') {
			flag_pause_servo1 = 1;
			d = 'x';
		}
		if(d == 'b') {
			if((*selected_recipe)[sj] == RECIPE_END) {
				sj = 0;
			}
			d = 'x';
		}
		if(d == 'l') {
			if(flag_pause_servo1 == 0) {
				if(curr_pos_servo1 != 0) {
					position(pos[curr_pos_servo1-1], PWM1);
					curr_pos_servo1 -= 1;
				}
			}
			d = 'x';
		}
		if(d == 'r') {
			if(flag_pause_servo1 == 0) {
				if(curr_pos_servo1 != 5) {
					position(pos[curr_pos_servo1+1], PWM1);
					curr_pos_servo1 += 1;
				}
			}
			d = 'x';
		}
		pthread_mutex_unlock(&mutex_d);
		if(flag_pause_servo1) {
			execute((*selected_recipe)[sj], PWM1);
			if(sj+1 < SELECTED_RECIPE_LENGTH)
				sj++;
			sleep(1);
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
    printf("Welcome to the QNX Momentics IDE\n");
    pthread_t servo0, servo1;
    Servo s0, s1;

    s0.id = 0;
    s1.id = 1;

    getRootPermissions();

    printRegister(CONTROL, "CONTROL");
    printRegister(PORT_B, "PORT_B");

    // Set Port B to output mode
    setBitInRegister(CONTROL, PORT_B_MODE, OUTPUT);

    set_conio_terminal_mode();

	pthread_create(&servo0, NULL, startServo0, (void*) &s0);
	pthread_create(&servo1, NULL, startServo1, (void*) &s1);

    for(;;) {
		while (!kbhit()) {}
		pthread_mutex_lock(&mutex_c);
		c = (char)getch();
		pthread_mutex_unlock(&mutex_c);
		pthread_mutex_lock(&mutex_d);
		d = (char)getch();
		pthread_mutex_unlock(&mutex_d);
		usleep(1000 * 1000);
		//sleep(1);
    }

    return EXIT_SUCCESS;
}
