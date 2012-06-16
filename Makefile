CC=gcc
#CFLAGS=-std=gnu99 -Wall -pthread -ggdb -Os
CFLAGS=-std=gnu99 -Wall -pthread -ggdb -lm
LFLAGS=-llvm2cmd -pthread

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
