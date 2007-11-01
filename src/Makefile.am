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

vagalume_LDADD = $(GTK2_LIBS) $(GSTREAMER_LIBS) $(LIBXML2_LIBS) \
	$(LIBCURL_LIBS) $(HILDON_LIBS) $(OSSO_LIBS)

vagalume_CFLAGS= $(GTK2_CFLAGS) $(GSTREAMER_CFLAGS) $(LIBXML2_CFLAGS) \
	$(LIBCURL_CFLAGS) $(HILDON_CFLAGS) $(OSSO_CFLAGS)

vagalume_SOURCES = \
	audio.c audio.h \
	controller.c controller.h \
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
