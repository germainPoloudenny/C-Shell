all : myls myps mysh
CC=gcc
CFLAGS=-Wall -g -pthread -fcommon

myls.o: myls.c utils.h
	$(CC) -c -o $@ $< $(CFLAGS)

myls: myls.c utils.c
	$(CC) -o $@ $^ $(CFLAGS)
	
myps.o: myps.c utils.h
	$(CC) -c -o $@ $< $(CFLAGS)

myps: myps.c utils.c
	$(CC) -o $@ $^ $(CFLAGS)

mysh.o: mysh.c utils.h job.h  variables.h
	$(CC) -c -o $@ $< $(CFLAGS)

mysh: mysh.c utils.c job.c  variables.c
	$(CC) -o $@ $^ $(CFLAGS)