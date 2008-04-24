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
	debian/files \
	debian/vagalume.postinst.debhelper \
	debian/vagalume.postrm.debhelper \
	debian/vagalume.substvars \
	depcomp \
	install-sh \
	ltmain.sh \
	missing \
	mkinstalldirs

SUBDIRS = src

icon_DATA = data/icons/vagalume.png data/icons/vagalume.xpm
icon48_DATA = data/icons/48x48/*
appdata_DATA = data/$(PLATFORM)/*
desktopentry_DATA = data/vagalume.desktop

if MAEMO_PLATFORM
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
man1_MANS = vagalume.1
endif

if HAVE_DBUS_SUPPORT
serviceentry_DATA = data/vagalume.service
endif

if HAVE_MAEMO_SB_PLUGIN
sbplugindesktopentry_DATA = data/vagalume-sb-plugin.desktop
endif
