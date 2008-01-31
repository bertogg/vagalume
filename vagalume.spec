
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
%{_datadir}/vagalume/ban.png
%{_datadir}/vagalume/dload.png
%{_datadir}/vagalume/love.png
%{_datadir}/vagalume/next.png
%{_datadir}/vagalume/play.png
%{_datadir}/vagalume/stop.png
%{_datadir}/vagalume/addplist.png
%{_datadir}/vagalume/addplist_pressed.png
%{_datadir}/vagalume/ban_pressed.png
%{_datadir}/vagalume/cover.png
%{_datadir}/vagalume/dload_pressed.png
%{_datadir}/vagalume/love_pressed.png
%{_datadir}/vagalume/next_pressed.png
%{_datadir}/vagalume/play_pressed.png
%{_datadir}/vagalume/recommend.png
%{_datadir}/vagalume/recommend_pressed.png
%{_datadir}/vagalume/stop_pressed.png
%{_datadir}/vagalume/tag.png
%{_datadir}/vagalume/tag_pressed.png


%changelog
* Mon Jan 28 2008 Tim Wegener <twegener@madabar.com> - 0.5-1
- Updated for 0.5.

* Fri Jan 25 2008 Tim Wegener <twegener@madabar.com> - 0.4-1
- Initial build.
