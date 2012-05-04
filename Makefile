CC=gcc
CFLAGS=-std=gnu99 -Wall -ggdb -Os
LFLAGS=-llvm2cmd

lvmtsd: lvmtsd.c lvmls.o
	$(CC) $(CFLAGS) lvmtsd.c lvmls.o $(LFLAGS) -o lvmtsd

lvmls.o: lvmls.c
	$(CC) $(CFLAGS) -c lvmls.c

lvmtscd: lvmtscd.c
	$(CC) $(CFLAGS) lvmtscd.c -o lvmtscd
