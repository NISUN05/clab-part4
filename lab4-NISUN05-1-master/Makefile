CC = gcc
CFLAGS = -Wall -Wno-unused-result -std=gnu99 -Og -g

OBJS = mdriver.o memlib.o 

all: implicit-unittest implicit-mdriver buddy-unittest buddy-mdriver

submitfiles:
	zip submitfiles.zip *.c *.h Makefile

implicit-unittest: memlib.o mm-implicit.o mm-implicit-unittest.o
	$(CC) $(CFLAGS) -o $@ $^

implicit-mdriver: $(OBJS) mm-implicit.o
	$(CC) $(CFLAGS) -o $@ $^

buddy-unittest: memlib.o mm-buddy.o mm-buddy-unittest.o
	$(CC) $(CFLAGS) -o $@ $^

buddy-mdriver: $(OBJS) mm-buddy.o
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.c %.h
	$(CC) $(CFLAGS) -c ${<}

clean:
	rm -f *~ *.o implicit-mdriver implicit-unittest buddy-mdriver buddy-unittest


