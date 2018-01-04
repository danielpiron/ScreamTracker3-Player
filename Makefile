CC=gcc
CFLAGS=--ansi -Wall -Werror -Wextra -Wpedantic -g
BINDIR=bin
SRCDIR=src

.PHONY : all
all: $(BINDIR)/s3mplay

.PHONY : clean
clean:
	rm -fr $(BINDIR)
	rm *.o

$(BINDIR)/s3mplay: s3mplay.o s3m.o s3mload.o
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/s3mplay s3mload.o s3mplay.o s3m.o -lportaudio

s3mload.o: $(SRCDIR)/s3mload.c $(SRCDIR)/s3m.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/s3mload.c

s3mplay.o: $(SRCDIR)/s3mplay.c $(SRCDIR)/s3m.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/s3mplay.c

s3m.o: $(SRCDIR)/s3m.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/s3m.c
