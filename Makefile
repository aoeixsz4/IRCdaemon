CFLAGS = -ggdb -c -Wall
LDFLAGS = -ggdb -lev
CC = gcc
RM = rm
OBJS = main.o irc.o net.o hash.o myev.o

all: ircdaemon

ircdaemon: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

irc.o: irc.c
	$(CC) $(CFLAGS) -o $@ $<

core.o: core.c
	$(CC) $(CFLAGS) -o $@ $<

net.o: net.c
	$(CC) $(CFLAGS) -o $@ $<

hash.o: hash.c
	$(CC) $(CFLAGS) -o $@ $<

myev.o: myev.c
	$(CC) $(CFLAGS) -o $@ $<

proper: clean
	$(RM) ircdaemon

clean:
	$(RM) *.o

main.c irc.c core.c: irc.h
core.c net.c myev.c: net.h
hash.c core.c irc.c: hash.h
core.c myev.c: myev.h
myev.h: irc.h
user.h: net.h
core.h: hash.h
