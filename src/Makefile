
CFLAGS=-O0 -g -Wall $(shell pkg-config --cflags gtk+-2.0 gstreamer-0.10)
LDFLAGS=$(shell pkg-config --libs gtk+-2.0 gstreamer-0.10 libcurl) -lgcrypt

default: main

main: main.o protocol.o

clean:
	rm -f main *.o
