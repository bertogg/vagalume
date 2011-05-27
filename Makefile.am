#
# This file is part of Vagalume and is published under the GNU GPLv3.
#
# Copyright (C) 2007-2011 Igalia, S.L.
#
# See the README file for more details.
#

EXTRA_DIST = \
	TRANSLATORS \
	autogen.sh \
	data/buttons \
	data/cover \
	data/icons \
	data/servers.xml.in \
	data/vagalume-rfs.sh.in \
	data/vagalume-sb-plugin.desktop \
	data/vagalume.conf.in \
	data/vagalume.desktop.in.in \
	data/vagalume.desktop.maemo \
	data/vagalume.desktop.moblin \
	data/vagalume.service.in \
	mkinstalldirs \
	scripts/vagalumectl \
	vagalume.1 \
	vagalumectl.1

SUBDIRS = src po
manpages = vagalume.1

pixmap_DATA = data/icons/vagalume.xpm
icon48_DATA = data/icons/48x48/*
appdata_DATA = data/cover/$(SCR_RESOLUTION)/cover.png data/servers.xml

desktopentry_DATA = data/vagalume.desktop
@INTLTOOL_DESKTOP_RULE@

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
themebigicon_DATA = data/buttons/big/*.png
themesmallicon_DATA = data/buttons/small/*.png
backupconfdir = @BACKUP_CONFIGURATION_DIR@
backupconf_DATA = data/vagalume.conf
restoresettingsexecscriptdir = @RESTORE_SETTINGS_SCRIPTS_DIR@
restoresettingsexecscript_SCRIPTS = data/vagalume-rfs.sh
else
themebigicon_DATA = data/buttons/big/*.svg
themesmallicon_DATA = data/buttons/small/*.svg
man1_MANS = $(manpages)
endif

edit = sed -e 's,@BINDIR\@,$(bindir),g' -e 's,@PIXMAPDIR\@,$(pixmapdir),g'

data/vagalume.service: Makefile data/vagalume.service.in
	rm -f data/vagalume.service
	$(edit) data/vagalume.service.in > data/vagalume.service

data/vagalume.desktop.in: Makefile data/vagalume.desktop.in.in
	rm -f data/vagalume.desktop.in
	$(edit) data/vagalume.desktop.in.in > data/vagalume.desktop.in
	if test -f "data/vagalume.desktop.$(PLATFORM)" ; then \
	   cat "data/vagalume.desktop.$(PLATFORM)" >> data/vagalume.desktop.in ; \
	fi
	if test "x$(PLATFORM)" = "xmaemo" -a "x$(PLATFORMVERS)" = "x5"; then \
	   rm -f data/vagalume.desktop.tmp ; \
	   mv data/vagalume.desktop.in data/vagalume.desktop.tmp ; \
	   sed s/^_Name=.*/Name=Vagalume/ data/vagalume.desktop.tmp > \
		data/vagalume.desktop.in ; \
	fi

data/servers.xml: Makefile data/servers.xml.in
	rm -f data/servers.xml
	if test "x$(DISABLE_LAST_FM)" = "xyes"; then \
	   sed '/BEGIN_LAST_FM/,/END_LAST_FM/d' \
	      data/servers.xml.in > data/servers.xml ; \
	else \
	   grep -v '_LAST_FM' data/servers.xml.in > data/servers.xml ; \
	fi

install-exec-hook:
	if test "$(PLATFORM)" = "maemo"; then \
	   $(mkinstalldirs) "$(DESTDIR)/@CLEAR_DEVICE_SCRIPTS_DIR@" ; \
	   $(LN_S) "@RESTORE_SETTINGS_SCRIPTS_DIR@/vagalume-rfs.sh" \
	      "$(DESTDIR)/@CLEAR_DEVICE_SCRIPTS_DIR@/vagalume-cud.sh" ; \
	   if test "x$(PLATFORMVERS)" = "x5"; then \
		ln -s /opt/vagalume/bin/vagalume $(DESTDIR)/usr/bin ; \
		ln -s /opt/vagalume/bin/vagalumectl $(DESTDIR)/usr/bin ; \
	   fi ; \
	fi

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		( cd "$(top_srcdir)" && \
		  $(top_srcdir)/missing --run git log --stat ) > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		     echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi
