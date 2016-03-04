all:
	gcc -c timer.c -o timer.o
	gcc -c queue.c -o queue.o
	gcc -c project4.c -o project4.o
ifeq ($(shell uname), Darwin)
	gcc timer.o queue.o project4.o -o project4
else
	gcc timer.o queue.o project4.o -lpthread -pthread -lrt -o project4
endif

clean:
	rm timer.o queue.o project4.o project4
