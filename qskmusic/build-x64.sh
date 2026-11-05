#!/bin/bash
set -e
set -x

QTDIR=/opt/qt/6.7.3/gcc_64
if [ ! -d "$QTDIR" ]; then
	# macos
	QTDIR=/opt/qt/6.7.3/macos
fi
export LD_LIBRARY_PATH=$QTDIR/lib

# ── i18n：lrelease .ts → .qm（.qrc 引用的 qm 必须先于 configure 存在） ──
( cd i18n && for f in qskmusic_en qskmusic_zh_CN; do
    "$QTDIR/bin/lrelease" "$f.ts" -qm "$f.qm"
  done )

mkdir -p build-x64 && cd build-x64
$QTDIR/bin/qt-cmake .. \
    -DQt6_DIR=$QTDIR/lib/cmake/Qt6 \
    -DCMAKE_PREFIX_PATH="$QTDIR;/opt/qt/qskinny" \
    -DCMAKE_BUILD_TYPE=Debug \
    -D CMAKE_CXX_FLAGS="-O1" -D CMAKE_C_FLAGS="-O1"

# hotfix (Linux only: macOS doesn't have these libs, and BSD sed needs different -i syntax)
if [ "$(uname)" != "Darwin" ]; then
    sed -i 's/-lQt6Qml -lQt6Quick -lQt6OpenGL//g' CMakeFiles/qskmusic.dir/link.txt 2>/dev/null || true
fi

#make -j$(nproc)
make -j1
echo "=== done: LD_LIBRARY_PATH=/opt/qt/qskinny/lib/qskinny:$QTDIR/lib ./build-x64/qskmusic ==="
