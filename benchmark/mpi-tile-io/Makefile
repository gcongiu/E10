#CC=/usr/mpi/gcc/openmpi-1.6.5/bin/mpicc
.PHONY: mpi-tile-io
CC=mpicc
CFLAGS=-Wall -g -O2 -DHAVE_GETOPT_LONG
LIBS=-lmpiwrap -ljson -llmpe -lmpe

all: mpi-tile-io

mpi-tile-io: mpi-tile-io.c
	$(CC) $(CFLAGS) mpi-tile-io.c -o $@ $(LIBS)

clean:
	rm -f mpi-tile-io *.o *~
