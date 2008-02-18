#!/bin/sh

set -e
libtoolize --automake --copy --force
aclocal
autoconf --force
autoheader --force
automake --add-missing --copy --force-missing --foreign
