#
# This file is part of Vagalume and is published under the GNU GPLv3.
#
# Copyright (C) 2007-2010 Igalia, S.L.
#
# See the README file for more details.
#

bin_PROGRAMS = vagalume

vagalume_LDADD = $(EXTRA_LIBS)

vagalume_CFLAGS= $(EXTRA_CFLAGS) -DAPP_DATA_DIR=\"$(appdatadir)\" \
	-DPIXMAP_DIR=\"$(pixmapdir)\" -DICON48_DIR=\"$(icon48dir)\"

vagalume_SOURCES = \
	audio.c audio.h \
	compat.c compat.h \
	connection.h \
	controller.c controller.h \
	dlwin.c dlwin.h \
	globaldefs.h \
	http.c http.h \
	lastfm-ws.c lastfm-ws.h \
	main.c \
	playlist.c playlist.h \
	protocol.c protocol.h \
	radio.c radio.h \
	scrobbler.c scrobbler.h \
	uimisc.c uimisc.h \
	userconfig.c userconfig.h \
	util.c util.h \
	vgl-bookmark-mgr.c vgl-bookmark-mgr.h \
	vgl-bookmark-window.c vgl-bookmark-window.h \
	vgl-main-menu.h \
	vgl-main-window.c vgl-main-window.h \
	vgl-object.c vgl-object.h \
	vgl-server.c vgl-server.h \
	xmlrpc.c xmlrpc.h

BUILT_SOURCES = marshal.c marshal.h
nodist_vagalume_SOURCES = marshal.c marshal.h
EXTRA_DIST = marshal.list

marshal.h: marshal.list
	glib-genmarshal --header \
		--prefix=vgl_marshal marshal.list > marshal.h

marshal.c: marshal.list
	glib-genmarshal --body \
		--prefix=vgl_marshal marshal.list > marshal.c

if HILDON_2_2
vagalume_SOURCES += uimisc-hildon22.c vgl-main-menu-hildon22.c
else
vagalume_SOURCES += uimisc-gtk.c vgl-main-menu-gtk.c
endif

if SET_IM_STATUS
vagalume_SOURCES += imstatus.c imstatus.h
endif

if USE_INTERNAL_MD5
vagalume_SOURCES += md5/md5.c md5/md5.h
endif

if HAVE_TRAY_ICON
vagalume_SOURCES += vgl-tray-icon.h vgl-tray-icon.c
endif

if HAVE_CONIC
vagalume_SOURCES += connection.c
endif

if HAVE_DBUS_SUPPORT
vagalume_SOURCES += dbus.c dbus.h
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
