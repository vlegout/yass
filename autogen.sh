#!/bin/sh -e

autoreconf --install --symlink

MYCFLAGS="-g -Wall -Wextra -O2"

./configure CFLAGS="${MYCFLAGS} ${CFLAGS}" $@

make tags > /dev/null
