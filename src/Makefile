ifeq ($(shell uname -m),arm)
	MAEMO=yes
endif

CFLAGS=-O0 -g -Wall $(shell pkg-config --cflags gtk+-2.0 \
	gstreamer-0.10 libxml-2.0)
LDFLAGS=$(shell pkg-config --libs gtk+-2.0 gstreamer-0.10 \
	libcurl libxml-2.0)

ifeq ($(MAEMO),yes)
	CFLAGS+=$(shell pkg-config --cflags hildon-libs) -DMAEMO
	LDFLAGS+=$(shell pkg-config --libs hildon-libs)
endif

default: main

main: main.o protocol.o playlist.o userconfig.o radio.o md5.o http.o \
	audio.o mainwin.o controller.o uimisc.o

clean:
	rm -f main *.o
