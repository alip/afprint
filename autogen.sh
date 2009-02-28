#!/bin/sh
# vim: set sw=4 et sts=4 tw=80 :

die() {
    echo "$@" >&2
    exit 1
}

echo ">>> rm -f config.cache"
rm -f config.cache
echo ">>> aclocal"
aclocal || die "aclocal failed"
echo ">>> autoheader"
autoheader || die "autoheader failed"
echo ">>> autoconf"
autoconf || die "autoconf failed"
echo ">>> automake --foreign --add-missing --copy"
automake --foreign --add-missing --copy || die "automake failed"
