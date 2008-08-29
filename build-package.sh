#!/bin/sh

set -e

if pkg-config --exists libosso; then # Compile for Maemo
    libossovers=`pkg-config libosso --modversion | cut -d . -f 1`
    if ! pkg-config --exists conic; then
        MAEMOVERS=2
    elif test "$libossovers" = "1"; then
        MAEMOVERS=3
    elif test "$libossovers" = "2"; then
        MAEMOVERS=4
    else
        echo "Unknown maemo version"
        exit 1
    fi

    if [ ! -f debian/control.debian ]; then
       # Make a backup
        cp -f debian/control debian/control.debian
    fi
    cp -f debian/control.maemo$MAEMOVERS debian/control
fi

dpkg-buildpackage -rfakeroot -b -uc

# Restore the backup
[ -f debian/control.debian ] && mv -f debian/control.debian debian/control
