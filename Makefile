CC=gcc
CFLAGS=-std=gnu99 -Wall -ggdb -Os
LFLAGS=-llvm2cmd

lvmtsd: lvmtsd.c lvmls.o
	$(CC) $(CFLAGS) lvmtsd.c lvmls.o $(LFLAGS) -o lvmtsd

lvmls.o: lvmls.c
	$(CC) $(CFLAGS) -c lvmls.c

lvmtscd: lvmtscd.c activity_stats.o
	$(CC) $(CFLAGS) lvmtscd.c activity_stats.o -o lvmtscd

activity_stats.o: activity_stats.c
	$(CC) $(CFLAGS) -c activity_stats.c

clean:
	rm -f lvmtscd lvmtsd *.o
