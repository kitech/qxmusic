#!/bin/sh
set -x
mkdir -p build-qt4 && cd build-qt4
QMAKE_EXTRA=""
if [ x"$1" = x"asan" ]; then QMAKE_EXTRA="CONFIG+=asan"; fi
qmake-qt4 $QMAKE_EXTRA ../qlmusic.pro
sed -i 's/\-O2/\-O1/g' Makefile
if [ x"$1" = x"c" ]; then
    make clean
fi
make
ret=$?
if [ x"$ret" = x"0" ] && [ -f "qlmusic" ]; then
    cp -v qlmusic q4music
fi
ls -lh q*music
