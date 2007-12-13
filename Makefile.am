#
# This file is part of Vagalume
#
# Copyright (C) 2007 Alberto Garcia Gonzalez
#
# This file is published under the GNU GPLv3.
#

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

icon_DATA = data/icons/vagalume.png
icon48_DATA = data/icons/48x48/*
appdata_DATA = data/buttons/*
desktopentry_DATA = data/vagalume.desktop

if HAVE_DBUS_SUPPORT
serviceentry_DATA = data/vagalume.service
endif
