
CFLAGS=-O0 -g -Wall $(shell pkg-config --cflags gtk+-2.0 \
	gstreamer-0.10 libxml-2.0)
LDFLAGS=$(shell pkg-config --libs gtk+-2.0 gstreamer-0.10 \
	libcurl libxml-2.0) -lgcrypt

default: main

main: main.o protocol.o playlist.o userconfig.o radio.o

clean:
	rm -f main *.o
