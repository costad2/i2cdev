#!/bin/bash

die() {
    echo "$@" >&2
    exit 1
}

BASEDIR="$(dirname "$0")"

cd "$BASEDIR" || die "Could not change into base directory $BASEDIR"

autoreconf -fi || die "Error during autoreconf"
rm -Rf autom4te.cache;

echo "running ./configure && make && make install should configure, build, and install this package."

