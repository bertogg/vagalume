
Summary: GTK+-based client for the Last.fm online radio service
Name: vagalume
Version: 0.7.1
Release: 1
License: GPL3
Group: Applications/Multimedia
URL: http://vagalume.igalia.com/
Source0: %{name}_%{version}-1.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: gcc, gtk2-devel >= 2.10, gstreamer-devel >= 0.10, curl-devel, libxml2-devel >= 2.0, libnotify-devel, dbus-glib-devel, libtool
Requires: gtk2 >= 2.10, gstreamer >= 0.10, curl, libxml2 >= 2.0, libnotify, dbus-glib

%description
Vagalume is a Last.fm client designed for the GNOME desktop
environment. It's small and provides the basic Last.fm features, such
as scrobbling, tags, recommendations, etc. Vagalume is also designed
to work in the Maemo platform, used by some Nokia devices such as the
Nokia 770, N800 and N810

%prep
%setup -q

%build
[ -x configure ] || ./autogen.sh
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=%{buildroot} install

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc README COPYING
%{_bindir}/vagalume
%{_bindir}/vagalumectl
%{_datadir}/applications/vagalume.desktop
%{_datadir}/dbus-1/services/vagalume.service
%{_datadir}/icons/hicolor/48x48/apps/vagalume.png
%{_datadir}/pixmaps/vagalume.xpm
%{_datadir}/vagalume/cover.png
%{_datadir}/vagalume/icons/hicolor/scalable/actions/accessories-text-editor.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/document-save.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/emblem-favorite.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/list-add.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/mail-message-new.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/media-playback-start.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/media-playback-stop.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/media-skip-forward.svg
%{_datadir}/vagalume/icons/hicolor/scalable/actions/process-stop.svg
%{_mandir}/man1/vagalume.1.gz
%{_mandir}/man1/vagalumectl.1.gz
%{_datadir}/locale/de/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/es/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/fi/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/fr/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/gl/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/it/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/lv/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/pt/LC_MESSAGES/vagalume.mo
%{_datadir}/locale/pt_BR/LC_MESSAGES/vagalume.mo


%changelog
* Thu Feb 26 2009 Alberto Garcia <agarcia@igalia.com> - 0.7.1-1
- Updated for 0.7.1.

* Mon Sep 01 2008 Alberto Garcia <agarcia@igalia.com> - 0.7-1
- Updated for 0.7.

* Fri May 09 2008 Tim Wegener <twegener@madabar.com> - 0.6-1
- Updated for 0.6.

* Fri Feb 15 2008 Alberto Garcia <agarcia@igalia.com> - 0.5.1-1
- Updated for 0.5.1.

* Mon Jan 28 2008 Tim Wegener <twegener@madabar.com> - 0.5-1
- Updated for 0.5.

* Fri Jan 25 2008 Tim Wegener <twegener@madabar.com> - 0.4-1
- Initial build.
