CC = gcc
CFLAGS = -Wall -Werror

all: emulator

emulator: emulator.o queue.o linkstate.o
	$(CC) emulator.o queue.o linkstate.o -o emulator

emulator.o: emulator.c 
	$(CC) $(CFLAGS) -c emulator.c

queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c

linkstate.o: linkstate.c
	$(CC) $(CFLAGS) -c linkstate.c

clean:
	rm -rf *.o
	rm -rf emulator
