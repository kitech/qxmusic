#!/bin/bash
set -e
export JAVA_HOME=/opt/jdk-17.0.13+11
export ANDROID_SDK_ROOT=/opt/android-sdk
export ANDROID_NDK_ROOT=/opt/android-ndk-r26b

APP_VERSION_CODE=$(git rev-list --count HEAD)
if [ -z "$APP_VERSION_CODE" ]; then APP_VERSION_CODE=1; fi
cat > src/version_config.h <<H
#ifndef VERSION_CONFIG_H
#define VERSION_CONFIG_H
#define APP_VERSION_NAME "0.1"
#define APP_VERSION_CODE ${APP_VERSION_CODE}
#endif
H
echo "[build-android.sh] versionConfig: 0.1 (${APP_VERSION_CODE})"

QT_ANDROID=/opt/qt/6.7.3/android_arm64_v8a
QSK_ANDROID=/opt/qt/qskinny-arm64
QT_HOST=/opt/qt/6.7.3/gcc_64

BUILD_DIR=build-android
# rm -rf "$BUILD_DIR"  # 保留 cmake 缓存以支持增量编译

# ── i18n：lrelease .ts → .qm（.qrc 引用的 qm 必须先于 configure 存在） ──
( cd i18n && for f in *.ts; do
    "$QT_HOST/bin/lrelease" "$f" -qm "${f%.ts}.qm"
  done )

# 1. cmake configure via qt-cmake
"$QT_ANDROID/bin/qt-cmake" -S . -B "$BUILD_DIR" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_PREFIX_PATH="$QSK_ANDROID" \
    -DQSkinny_DIR="$QSK_ANDROID/lib/cmake/QSkinny" \
    -DQT_HOST_PATH="$QT_HOST" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 2. build native .so only (not apk target)
cmake --build "$BUILD_DIR" -j1 --target qskmusic

# 3. prepare android-build dir (copy .so + Qt deps, no Gradle)
APK_DIR="$BUILD_DIR/android-build"
LIB_DIR="$APK_DIR/libs/arm64-v8a"
mkdir -p "$LIB_DIR"
cp "$BUILD_DIR/libqskmusic_arm64-v8a.so" "$LIB_DIR/"

# 平铺复制 qskinny 皮肤插件到 JNI lib 根目录（Android 无子目录）
cp "$QSK_ANDROID/lib/qskinny/plugins/skins/"*.so "$LIB_DIR/"
cp "$QSK_ANDROID/lib/qskinny/plugins/platforminputcontexts/"*.so "$LIB_DIR/"

# 复制 extras 共享库到 APK（libgoso 由 cmake extras 段按 GOOS=android 自动构建）
cp "$BUILD_DIR/libgoso.so" "$LIB_DIR/"
cp "$BUILD_DIR/libcso.so" "$LIB_DIR/"

"$QT_HOST/bin/androiddeployqt" \
    --input "$BUILD_DIR/android-qskmusic-deployment-settings.json" \
    --output "$APK_DIR" \
    --deployment bundled \
    --builddir "$BUILD_DIR" \
    --aux-mode

# 复制应用图标到 Android res
mkdir -p "$APK_DIR/res/drawable"
cp app_icon.png "$APK_DIR/res/drawable/ic_launcher.png"

# 移除 renderscript.srcDirs 配置（build-tools 34+ 不再支持）
sed -i '/renderscript\.srcDirs/d' "$APK_DIR/build.gradle"

# 注入版本号（defaultConfig 覆盖 Manifest 默认值；先删已注入行再追加，保证幂等）
sed -i '/^        versionCode /d; /^        versionName /d' "$APK_DIR/build.gradle"
sed -i "/defaultConfig {/a\\        versionCode ${APP_VERSION_CODE}\n        versionName \"0.1\"" "$APK_DIR/build.gradle"

# Qt 模板生成 useLegacyPackaging true（.so DEFLATE 压缩，安装时解压到磁盘）。
# Qt 6.9 之前不支持直接从 APK 加载未压缩(STORED)的 .so —— 必须保持 true。

# 4. append Qt-specific properties (missing from --aux-mode)
cat >> "$APK_DIR/gradle.properties" <<PROPS
# 模板默认 -Xmx386m 过小，packageDebug 打包大 .so 时 OOM；
# java.util.Properties 后值覆盖前值，此行覆盖模板头部的旧 jvmargs
org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=1024m -Dfile.encoding=UTF-8
androidBuildToolsVersion=34.0.0
androidCompileSdkVersion=android-33
androidNdkVersion=26.1.10909125
androidPackageName=io.fedlet.qskmusic
buildDir=build
qtAndroidDir=${QT_ANDROID}/src/android/java
qtMinSdkVersion=23
qtTargetAbiList=arm64-v8a
qtTargetSdkVersion=34
PROPS

# 5. patch Gradle heap (low-memory device)
sed -i 's/-Xmx[0-9]*m/-Xmx2048m/' "$APK_DIR/gradle.properties"

# 6. build debug APK (auto-signed with Android debug key)
GRADLE_OPTS="-Xmx2048m" "$APK_DIR/gradlew" --no-daemon --max-workers=2 -p "$APK_DIR" assembleDebug

# 7. verify output
APK=$(find "$APK_DIR/build/outputs" -name "*.apk" 2>/dev/null | head -1)
if [ -n "$APK" ]; then
    echo "=== APK built: $APK ==="
    ls -lh "$APK"
fi

find build-android -name *qskmusic*.so | xargs ls -lh
objdump -x build-android/android-build/libs/arm64-v8a/libqskmusic_arm64-v8a.so|grep NEEDED|grep -v libQt|grep -v libc|grep -v libm|grep -v libc
