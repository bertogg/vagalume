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

if WINDOWS_PLATFORM
vagalume_CFLAGS= $(EXTRA_CFLAGS) -DAPP_DATA_DIR=\"../share/vagalume\" \
	-DPIXMAP_DIR=\"../share/pixmaps\" -DICON48_DIR=\"../share/icons/hicolor/48x48/apps\"
else
vagalume_CFLAGS= $(EXTRA_CFLAGS) -DAPP_DATA_DIR=\"$(appdatadir)\" \
	-DPIXMAP_DIR=\"$(pixmapdir)\" -DICON48_DIR=\"$(icon48dir)\"
endif

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
	metadata.c metadata.h \
	playlist.c playlist.h \
	protocol.c protocol.h \
	radio.c radio.h \
	scrobbler.c scrobbler.h \
	uimisc.c uimisc.h \
	userconfig.c userconfig.h \
	util.c util.h \
	vgl-bookmark-mgr.c vgl-bookmark-mgr.h \
	vgl-bookmark-window.c vgl-bookmark-window.h \
	vgl-main-window.c vgl-main-window.h \
	xmlrpc.c xmlrpc.h

if WINDOWS_PLATFORM
vagalume_SOURCES += windows/resource.rc
vagalume_LDFLAGS = -mwindows
endif
	
if SET_IM_STATUS
vagalume_SOURCES += imstatus.c
endif

if USE_INTERNAL_MD5
vagalume_SOURCES += md5/md5.c md5/md5.h
endif

if HAVE_TRAY_ICON
if WINDOWS_PLATFORM
vagalume_SOURCES += vgl-tray-icon-win.c vgl-tray-icon-win.h
else
vagalume_SOURCES += vgl-tray-icon.h vgl-tray-icon.c
endif
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
	vgl-sb-plugin.c vgl-sb-plugin.h

libvagalumesbplugin_la_LIBADD = $(SBPLUGIN_LIBS)

libvagalumesbplugin_la_CFLAGS = $(SBPLUGIN_CFLAGS) \
	-DAPP_DATA_DIR=\"$(appdatadir)\" \
	-DPIXMAP_DIR=\"$(pixmapdir)\"
endif

# These two files are not needed
install-data-hook:
	if test "x$(sbplugindir)" != "x"; then \
	   rm -f "$(DESTDIR)/$(sbplugindir)/libvagalumesbplugin.a" ; \
	   rm -f "$(DESTDIR)/$(sbplugindir)/libvagalumesbplugin.la" ; \
	fi

if WINDOWS_PLATFORM
.rc.o:
	windres -I windows $< $@
endif