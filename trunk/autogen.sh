#!/bin/sh

glib-gettextize -f -c
intltoolize --copy --force --automake

ACLOCAL=aclocal AUTOMAKE=automake autoreconf -v --install --force || exit 1
./configure --enable-maintainer-mode "$@"
