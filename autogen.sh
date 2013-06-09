#!/bin/sh

set -ex

libtoolize --copy --force --automake
aclocal
autoconf --force
autoheader --force
automake --copy --force-missing --add-missing
glib-gettextize --copy --force
intltoolize --copy --force --automake

if test x$NOCONFIGURE = x; then
    ./configure "$@"
else
    echo Skipping configure process.
fi
