#!/bin/sh

set -e

if pkg-config --exists libosso; then # Compile for Maemo
    if [ ! -f debian/control.debian ]; then
       # Make a backup
        cp -f debian/control debian/control.debian
    fi
    cp -f debian/control.maemo debian/control
fi

[ -x configure ] || ./autogen.sh

dpkg-buildpackage -rfakeroot -b -uc

# Restore the backup
[ -f debian/control.debian ] && mv -f debian/control.debian debian/control
