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

$(BINDIR)/s3mplay: s3mplay.o s3m.o
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/s3mplay s3mplay.o s3m.o -lportaudio

s3mplay.o: $(SRCDIR)/s3mplay.c $(SRCDIR)/s3m.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/s3mplay.c

s3m.o: $(SRCDIR)/s3m.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/s3m.c
