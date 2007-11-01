#!/bin/sh

set -x

if pkg-config --exists libosso; then # Compie for Maemo
    if [ ! -f debian/control.debian ]; then
       # Make a backup
        cp -f debian/control debian/control.debian
    fi
    cp -f debian/control.maemo debian/control
fi

dpkg-buildpackage -rfakeroot -b -uc
