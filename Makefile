lvmtsd: lvmtsd.c lvmls.o
	gcc -std=gnu99 -Wall lvmtsd.c lvmls.o -llvm2cmd -o lvmtsd -Os -ggdb

lvmls.o: lvmls.c
	gcc -std=gnu99 -Wall -c lvmls.c -Os -ggdb
