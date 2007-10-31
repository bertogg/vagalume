#!/bin/sh

set -x
libtoolize --automake --copy --force
aclocal
autoconf --force
autoheader --force
automake --add-missing --copy --force-missing --foreign
