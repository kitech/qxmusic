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

# ── libncm：NCM 客户端共享库（Go c-shared）──
# 产物布局（go163-music-core 约定，各目标互不覆盖）：
#   linux 本机 → out/libncm.so      （build-linux.sh）
#   macOS 本机 → out/libncm.dylib   （build-macos.sh，install_name=@rpath）
#   交叉 linux → out/linux/<arch>/  （build-linux.sh --arch）
#   android    → out/android/<abi>/ （build-android.sh）
# 每次全量重跑 go build（增量缓存，源码变更自动带上）
if [ "$(uname)" = "Darwin" ]; then
    ( cd ../go163-music-core && scripts/build-macos.sh )
    NCM_OUT=../go163-music-core/out/libncm.dylib
else
    ( cd ../go163-music-core && scripts/build-linux.sh )
    NCM_OUT=../go163-music-core/out/libncm.so
fi

# 架构守卫：宿主产物必须与 uname 匹配（防错拿交叉产物链进桌面包）
# 注：file 输出 Linux 用 "x86-64"、mac 用 "x86_64"，两种都认
case "$(uname -m)" in
    x86_64)  NCM_ARCH_RE='x86-64|x86_64' ;;
    aarch64) NCM_ARCH_RE='aarch64' ;;
    arm64)   NCM_ARCH_RE='arm64' ;;   # mac Apple Silicon 的 uname -m
    *)       NCM_ARCH_RE='' ;;
esac
if [ -n "$NCM_ARCH_RE" ] && ! file "$NCM_OUT" | grep -Eq "$NCM_ARCH_RE"; then
    echo "FATAL: $NCM_OUT 架构不对（期望 $NCM_ARCH_RE）：" >&2
    file "$NCM_OUT" >&2
    exit 1
fi

mkdir -p build-x64
cp "$NCM_OUT" build-x64/    # IMPORTED 指向本目录，构建 rpath 直达
cd build-x64
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
echo "=== done: DYLD_LIBRARY_PATH=/opt/qt/qskinny/lib/qskinny:$QTDIR/lib ./build-x64/qskmusic ==="
