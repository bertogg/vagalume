#
# This file is part of Vagalume and is published under the GNU GPLv3.
#
# Copyright (C) 2007, 2008 Alberto Garcia Gonzalez
#
# See the README file for more details.
#

MAINTAINERCLEANFILES = \
	Makefile.in

bin_PROGRAMS = vagalume

vagalume_LDADD = $(EXTRA_LIBS)

vagalume_CFLAGS= $(EXTRA_CFLAGS) -DAPP_DATA_DIR=\"$(appdatadir)\" \
	-DICON_DIR=\"$(icondir)\" -DICON48_DIR=\"$(icon48dir)\"

vagalume_SOURCES = \
	audio.c audio.h \
	connection.h \
	controller.c controller.h \
	dbus.h \
	dlwin.c dlwin.h \
	globaldefs.h \
	http.c http.h \
	imstatus.h \
	main.c \
	mainwin.c mainwin.h \
	metadata.c metadata.h \
	playlist.c playlist.h \
	protocol.c protocol.h \
	radio.c radio.h \
	scrobbler.c scrobbler.h \
	uimisc.c uimisc.h \
	userconfig.c userconfig.h \
	util.c util.h \
	xmlrpc.c xmlrpc.h

if SET_IM_STATUS
vagalume_SOURCES += imstatus.c
endif

if USE_INTERNAL_MD5
vagalume_SOURCES += md5/md5.c md5/md5.h
endif

if HAVE_CONIC
vagalume_SOURCES += connection.c
endif

if HAVE_DBUS_SUPPORT
vagalume_SOURCES += dbus.c
endif

if HAVE_MAEMO_SB_PLUGIN
sbplugin_LTLIBRARIES = libvagalumesbplugin.la

libvagalumesbplugin_la_LDFLAGS = -module -avoid-version

libvagalumesbplugin_la_SOURCES = \
	dbus.h \
	globaldefs.h \
	vagalume-sb-plugin.c vagalume-sb-plugin.h

libvagalumesbplugin_la_LIBADD = $(SBPLUGIN_LIBS)

libvagalumesbplugin_la_CFLAGS = $(SBPLUGIN_CFLAGS) \
	-DAPP_DATA_DIR=\"$(appdatadir)\" \
	-DICON_DIR=\"$(icondir)\"
endif
