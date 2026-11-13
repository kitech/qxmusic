#!/bin/sh
set -e
set -x

export PKG_CONFIG_PATH=/opt/vcpkg/installed/x64-osx/lib/pkgconfig
QMAKE_EXTRA=""
if [ x"$1" = x"asan" ]; then
    QMAKE_EXTRA="CONFIG+=asan"
fi

# script must copy as buildosx{3,4,5,6}.sh
exename=$(basename $0)
qtnum=${exename: -4: 1}

if [ x"$qtnum" == x"4" ]; then
	# macports binary qt4 installation
	QMAKE=${QMAKE:-/opt/local/libexec/qt4/bin/qmake}
elif [ x"$qtnum" == x"6" ]; then
	QMAKE=${QMAKE:-/opt/qt/6.7.3/macos/bin/qmake}
else
	qtnum=5
	QMAKE=${QMAKE:-/opt/qt/5.15.2/clang_64/bin/qmake}
fi
builddir="build-osx${qtnum}"
mkdir -p "$builddir" && cd "$builddir"
"$QMAKE" -r $QMAKE_EXTRA ../
make

# package
mkdir -p qlmusic.app/Contents/Resources/lang
# bundle self-contained NCM dylib（存在才拷；无则与旧行为一致）
if [ -f ../go163-music-core/out/libncm.dylib ]; then
    mkdir -p qlmusic.app/Contents/Frameworks
    cp -f ../go163-music-core/out/libncm.dylib qlmusic.app/Contents/Frameworks/
fi
QT_TRANS=$("$QMAKE" -query QT_INSTALL_TRANSLATIONS)
cp -f "$QT_TRANS/qt_zh_CN.qm" "$QT_TRANS/qt_zh_TW.qm" qlmusic.app/Contents/Resources/ 2>/dev/null || true
cp -f lang/*.json qlmusic.app/Contents/Resources/lang/ 2>/dev/null || true
# imageformats 运行时插件（Qt4 的 jpeg/gif/tiff 不内置；缺 libqjpeg → 所有 jpg 封面解码失败）
QT_PLUGDIR=$("$QMAKE" -query QT_INSTALL_PLUGINS)
if [ -d "$QT_PLUGDIR/imageformats" ]; then
    mkdir -p qlmusic.app/Contents/PlugIns/imageformats
    cp -f "$QT_PLUGDIR/imageformats/"* qlmusic.app/Contents/PlugIns/imageformats/ 2>/dev/null || true
    # 插件自身的 Qt 依赖改指 bundle 内 Frameworks（Qt4 插件常带 @rpath 前缀）
    for p in qlmusic.app/Contents/PlugIns/imageformats/*; do
        install_name_tool -change @rpath/QtCore.framework/Versions/4/QtCore \
            @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore "$p" 2>/dev/null || true
        install_name_tool -change @rpath/QtGui.framework/Versions/4/QtGui \
            @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui "$p" 2>/dev/null || true
    done
fi
tar zcf ../qlmusic-qt4-osx-x64.tar.gz qlmusic.app/
ls -lh ../qlmusic-*.gz
ls -lh qlmusic.app/Contents/MacOS/
