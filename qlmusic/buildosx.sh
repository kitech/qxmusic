#!/bin/sh
set -e
set -x

export PKG_CONFIG_PATH=/opt/vcpkg/installed/x64-osx/lib/pkgconfig
QMAKE=${QMAKE:-/opt/qt/5.15.2/clang_64/bin/qmake}
QMAKE_EXTRA=""
if [ x"$1" = x"asan" ]; then
    QMAKE_EXTRA="CONFIG+=asan"
fi
"$QMAKE" -r $QMAKE_EXTRA
make

# package
mkdir -p qlmusic.app/Contents/Resources/lang
QT_TRANS=$("$QMAKE" -query QT_INSTALL_TRANSLATIONS)
cp -f "$QT_TRANS/qt_zh_CN.qm" "$QT_TRANS/qt_zh_TW.qm" qlmusic.app/Contents/Resources/ 2>/dev/null || true
cp -f lang/*.json qlmusic.app/Contents/Resources/lang/ 2>/dev/null || true
tar zcf qlmusic-qt5-osx-x64.tar.gz qlmusic.app/
ls -lh qlmusic-*.gz
ls -lh qlmusic.app/Contents/MacOS/
