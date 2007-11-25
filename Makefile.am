#
# This file is part of Vagalume
#
# Copyright (C) 2007 Alberto Garcia Gonzalez
#
# This file is published under the GNU GPLv3.
#

EXTRA_DIST = \
	autogen.sh \
	debian/changelog \
	debian/control \
	debian/copyright \
	debian/rules \
	debian/compat

MAINTAINERCLEANFILES = \
	Makefile.in \
	aclocal.m4 \
	compile \
	config.guess \
	config.h.in \
	config.h.in~ \
	config.sub \
	configure \
	depcomp \
	install-sh \
	ltmain.sh \
	missing

SUBDIRS = src


icondir = $(datadir)/pixmaps
icon_DATA = data/pixmaps/vagalume.png

appdata_DATA = data/pixmaps/*
desktopentry_DATA = data/vagalume.desktop

if HAVE_DBUS_SUPPORT
serviceentry_DATA = data/vagalume.service
endif
