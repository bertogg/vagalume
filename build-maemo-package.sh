#!/bin/sh

if [ ! -f debian/control.debian ]; then
    # Make a backup
    cp -f debian/control debian/control.debian
fi
cp -f debian/control.maemo debian/control

ENABLE_MAEMO=yes dpkg-buildpackage -rfakeroot -b -uc
