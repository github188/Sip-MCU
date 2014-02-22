#!/bin/sh
set -x
aclocal && libtoolize --copy --force && automake -a -c && autoconf

