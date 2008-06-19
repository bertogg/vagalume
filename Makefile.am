#
# This file is part of Vagalume and is published under the GNU GPLv3.
#
# Copyright (C) 2007, 2008 Alberto Garcia Gonzalez
#
# See the README file for more details.
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
	data/vagalume.desktop \
	data/vagalume.service \
	debian/files \
	debian/vagalume.debhelper.log \
	debian/vagalume.postinst.debhelper \
	debian/vagalume.postrm.debhelper \
	debian/vagalume.substvars \
	depcomp \
	install-sh \
	ltmain.sh \
	missing \
	mkinstalldirs \
	po/Makefile.in.in

SUBDIRS = src po
manpages = vagalume.1

pixmap_DATA = data/icons/vagalume.xpm
icon48_DATA = data/icons/48x48/*
desktopentry_DATA = data/vagalume.desktop
appdata_DATA = data/$(PLATFORM)/*

if HAVE_DBUS_SUPPORT
serviceentry_DATA = data/vagalume.service
bin_SCRIPTS = scripts/vagalumectl
manpages += vagalumectl.1
endif

if HAVE_MAEMO_SB_PLUGIN
sbplugindesktopentry_DATA = data/vagalume-sb-plugin.desktop
endif

if MAEMO_PLATFORM
icon26dir = $(datadir)/icons/hicolor/26x26/apps
icon26_DATA = data/icons/26x26/*
iconscalabledir = $(datadir)/icons/hicolor/scalable/apps
iconscalable_DATA = data/icons/scalable/*
themebigicondir = $(appdatadir)/icons/hicolor/scalable/apps
themebigicon_DATA = data/buttons/big/*.png
themesmallicondir = $(appdatadir)/icons/hicolor/50x50/apps
themesmallicon_DATA = data/buttons/small/*.png
backupconfdir = @BACKUP_CONFIGURATION_DIR@
backupconf_DATA = data/vagalume.conf
restoresettingsexecscriptdir = @RESTORE_SETTINGS_SCRIPTS_DIR@
restoresettingsexecscript_SCRIPTS = data/vagalume-rfs.sh
else
themeicondir = $(appdatadir)/icons/hicolor/scalable/actions
themeicon_DATA = data/buttons/big/*.svg data/buttons/small/*.svg
man1_MANS = $(manpages)
endif

edit = sed -e 's,@BINDIR\@,$(bindir),g' -e 's,@ICONDIR\@,$(icondir),g'

data/vagalume.service: Makefile data/vagalume.service.in
	rm -f data/vagalume.service data/vagalume.service.tmp
	$(edit) data/vagalume.service.in > data/vagalume.service.tmp
	mv data/vagalume.service.tmp data/vagalume.service

data/vagalume.desktop: Makefile data/vagalume.desktop.in
	rm -f data/vagalume.desktop data/vagalume.desktop.tmp
	$(edit) data/vagalume.desktop.in > data/vagalume.desktop.tmp
	mv data/vagalume.desktop.tmp data/vagalume.desktop

install-exec-hook:
	if test "$(PLATFORM)" = "maemo"; then \
	   $(mkinstalldirs) "$(DESTDIR)/@CLEAR_DEVICE_SCRIPTS_DIR@" ; \
	   $(LN_S) "@RESTORE_SETTINGS_SCRIPTS_DIR@/vagalume-rfs.sh" \
	      "$(DESTDIR)/@CLEAR_DEVICE_SCRIPTS_DIR@/vagalume-cud.sh" ; \
	fi
