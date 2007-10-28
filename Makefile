
default:
	make -C src

install:
	make -C src install
	mkdir -p $(DESTDIR)/usr/share/applications/hildon
	cp lastfm.desktop $(DESTDIR)/usr/share/applications/hildon
	mkdir -p $(DESTDIR)/usr/share/dbus-1/services
	cp lastfm.service $(DESTDIR)/usr/share/dbus-1/services

clean:
	make -C src clean
