#
# This file is part of Vagalume
#
# Copyright (C) 2007, 2008 Alberto Garcia Gonzalez
#
# This file is published under the GNU GPLv3.
#

MAINTAINERCLEANFILES = \
	Makefile.in \
	aclocal.m4 \
	build-stamp \
	compile \
	config.guess \
	config.h.in \
	config.h.in~ \
	config.sub \
	configure \
	debian/files \
	debian/vagalume.* \
	depcomp \
	install-sh \
	ltmain.sh \
	missing \
	mkinstalldirs

SUBDIRS = src

icon_DATA = data/icons/vagalume.png
icon48_DATA = data/icons/48x48/*
appdata_DATA = data/$(PLATFORM)/*
desktopentry_DATA = data/vagalume.desktop

if MAEMO_PLATFORM
themebigicondir = $(appdatadir)/icons/hicolor/scalable/apps
themebigicon_DATA = data/buttons/big/*.png
themesmallicondir = $(appdatadir)/icons/hicolor/50x50/apps
themesmallicon_DATA = data/buttons/small/*.png
else
themeicondir = $(appdatadir)/icons/hicolor/scalable/actions
themeicon_DATA = data/buttons/big/*.svg data/buttons/small/*.svg
endif

if HAVE_DBUS_SUPPORT
serviceentry_DATA = data/vagalume.service
endif

if HAVE_MAEMO_SB_PLUGIN
sbplugindesktopentry_DATA = data/vagalume-sb-plugin.desktop
endif
