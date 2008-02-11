
Summary: A small Last.fm client for Gnome and Maemo
Name: vagalume
Version: 0.5
Release: 1
License: GPL3
Group: Applications/Multimedia
URL: http://people.igalia.com/berto/
Source0: %{name}_%{version}-1.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: gcc, gtk2-devel, gstreamer-devel >= 0.10, curl-devel, libxml2-devel >= 2.0
Requires: gtk2, gstreamer >= 0.10, curl, libxml2 >= 2.0

%description
Vagalume is a Last.fm client designed for the Gnome desktop
environment. It's small and provides the basic Last.fm features, such
as scrobbling, tags, recommendations, etc. Vagalume is specially
designed to work in the Maemo platform, used by some Nokia devices
such as the Nokia 770, N800 and N810

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
%{_datadir}/applications/vagalume.desktop
%{_datadir}/icons/hicolor/48x48/apps/vagalume.png
%{_datadir}/pixmaps/vagalume.png
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


%changelog
* Mon Jan 28 2008 Tim Wegener <twegener@madabar.com> - 0.5-1
- Updated for 0.5.

* Fri Jan 25 2008 Tim Wegener <twegener@madabar.com> - 0.4-1
- Initial build.
