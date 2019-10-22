CC=gcc
MASTER_CFILE = oss.c
CHILD_CFILE = user.c

MASTER_OBJ=$(MASTER_CFILE:.c=.o)
CHILD_OBJ=$(CHILD_CFILE:.c=.o)

MASTER_EXE = oss
CHILD_EXE = child

CFLAGS = -g -Wall
MATH = -lm

HEADER_FILE = shared_mem.h


all: $(CHILD_EXE) $(MASTER_EXE)

$(CHILD_EXE): $(CHILD_OBJ)
	$(CC) $(CHILD_OBJ) -o $(CHILD_EXE) $(MATH)

$(MASTER_EXE): $(MASTER_OBJ)
	$(CC) $(MASTER_OBJ) -o $(MASTER_EXE) $(MATH)

%.o: %.c $(HEADER_FILE)
	$(CC) -c $(CFLAGS) $*.c -o $*.o

.PHONY: clean

clean:
	rm *.o $(CHILD_EXE) $(MASTER_EXE)
