#
# This file is part of Vagalume
#
# Copyright (C) 2007 Alberto Garcia Gonzalez
#
# This file is published under the GNU GPLv3.
#

MAINTAINERCLEANFILES = \
	Makefile.in

bin_PROGRAMS = vagalume

vagalume_LDADD = $(EXTRA_LIBS)

vagalume_CFLAGS= $(EXTRA_CFLAGS) -DAPP_DATA_DIR=\"$(appdatadir)\" \
	-DICON_DIR=\"$(icondir)\" -DICON48_DIR=\"$(icon48dir)\"

vagalume_SOURCES = \
	audio.c audio.h \
	controller.c controller.h \
	dbus.h \
	globaldefs.h \
	http.c http.h \
	main.c \
	mainwin.c mainwin.h \
	md5.c md5.h \
	playlist.c playlist.h \
	protocol.c protocol.h \
	radio.c radio.h \
	scrobbler.c scrobbler.h \
	uimisc.c uimisc.h \
	userconfig.c userconfig.h \
	util.c util.h \
	xmlrpc.c xmlrpc.h

if HAVE_DBUS_SUPPORT
vagalume_SOURCES += dbus.c
endif
