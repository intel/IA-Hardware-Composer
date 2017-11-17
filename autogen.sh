#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

mkdir -p m4

autoreconf -v --install -Itests/third_party/json-c/autoconf-archive/m4 || exit 1
cd $ORIGDIR || exit $?

if test -z "$NOCONFIGURE"; then
        $srcdir/configure "$@"
fi
