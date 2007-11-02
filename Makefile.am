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

install-data-local:
	if test -n "$(desktopentrydir)"; then \
	   $(mkinstalldirs) $(DESTDIR)/$(desktopentrydir) ; \
	   $(INSTALL) maemo/vagalume.desktop $(DESTDIR)/$(desktopentrydir) ; \
	fi
	if test -n "$(serviceentrydir)"; then \
	   $(mkinstalldirs) $(DESTDIR)/$(serviceentrydir) ; \
	   $(INSTALL) maemo/vagalume.service $(DESTDIR)/$(serviceentrydir) ; \
	fi
