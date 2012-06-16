CC=gcc
#CFLAGS=-std=gnu99 -Wall -pthread -ggdb -Os
CFLAGS=-std=gnu99 -Wall -pthread -ggdb -lm
LFLAGS=-llvm2cmd -pthread

all: lvmtsd lvmtscd lvmtscat

lvmtsd: lvmtsd.c lvmls.o
	$(CC) $(CFLAGS) lvmtsd.c lvmls.o $(LFLAGS) -o lvmtsd

lvmls.o: lvmls.c
	$(CC) $(CFLAGS) -c lvmls.c

lvmtscd: lvmtscd.c activity_stats.o
	$(CC) $(CFLAGS) lvmtscd.c activity_stats.o -o lvmtscd

lvmtscat: lvmtscat.c activity_stats.o
	$(CC) $(CFLAGS) lvmtscat.c activity_stats.o -o lvmtscat

activity_stats.o: activity_stats.c
	$(CC) $(CFLAGS) -c activity_stats.c

clean:
	rm -f lvmtscd lvmtscat lvmtsd *.o
