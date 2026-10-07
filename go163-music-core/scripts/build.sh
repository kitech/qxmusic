#!/usr/bin/env bash
#
# build.sh - 按目标平台分发到对应的构建脚本
#
# 用法:
#   scripts/build.sh                                  # 宿主平台 + 宿主架构
#   scripts/build.sh --arch arm64                     # 宿主平台 + 指定架构
#   scripts/build.sh --os android --arch all          # 全部 Android ABI
#   scripts/build.sh --os macos --arch arm64 SDKROOT=...
#   scripts/build.sh --list                          # 列出支持的目标
#
# 环境变量:
#   TARGET_OS     等价于 --os（保留兼容；两者都给时 --os 优先）
#   TARGET_ARCH   等价于 --arch（保留兼容；两者都给时 --arch 优先）
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 本脚本自己不做编译，只复用 _common.sh 里的 flag 注册表与错误输出，
# 好让 --help/--list 的呈现方式与各 build-*.sh 完全一致。
# shellcheck source=scripts/_common.sh
source "$ROOT/scripts/_common.sh"

flag "--os <os>" "目标平台: linux macos windows android auto（默认宿主；另接受 darwin/osx/win 与大小写不敏感）"
flag "--arch <arch>" "目标架构: amd64 arm64 armv7 armv6 armv5 386 auto"
flag "--list, -l" "列出支持的目标矩阵"
flag "-h, --help" "显示本帮助"

TARGET_OS="${TARGET_OS:-auto}"
ARCH="auto"

print_targets() {
cat <<'EOF'
构建目标 (c-shared)

  平台     --arch        产物             工具链要求
  -------  ------------  ---------------  --------------------------------------------
  linux    amd64         libncm.so        本机 gcc/clang
           arm64         libncm.so        aarch64-linux-gnu-gcc  (或 clang)
           armv7/armhf   libncm.so        arm-linux-gnueabihf-gcc
           armv6         libncm.so        arm-linux-gnueabihf-gcc, GOARM=6
           armv5/armsf   libncm.so        arm-linux-gnueabi-gcc,  GOARM=5 (软浮点)
           386           libncm.so        gcc-multilib
  macos    arm64         libncm.dylib     clang + Xcode CLT
           amd64         libncm.dylib     clang + Xcode CLT
           32 位 ARM     ——             不支持：Go 无 darwin/arm
  windows  amd64         ncm.dll          mingw-w64 (MSYS2) / x86_64-w64-mingw32-gcc
           arm64         ncm.dll          aarch64-w64-mingw32-gcc (较新版本才有)
           32 位 ARM     ——             不支持：无可用 mingw-w64 工具链
  android  arm64         libncm.so        NDK r21+  -> jniLibs/arm64-v8a     (64 位)
           armv7         libncm.so        NDK r21+  -> jniLibs/armeabi-v7a   (32 位)
           amd64         libncm.so        NDK       -> jniLibs/x86_64        (模拟器)
           386           libncm.so        NDK       -> jniLibs/x86           (模拟器)

  GOARCH=arm 必须与 GOARM 匹配（软/硬浮点 ABI），否则运行期 Illegal instruction:
    --arch armv7 -> GOARM=7 (VFPv3)    Android 仅支持此项（NDK r17+ 只有 v7a）
    --arch armv6 -> GOARM=6 (VFPv1)
    --arch armv5 -> GOARM=5 (softfloat)

  --arch 可重复，用于一次构建多个 ABI。

  Android 专用环境变量:
    ANDROID_NDK_ROOT    NDK 路径；未设置时从 ANDROID_SDK_ROOT/ndk/<最高版本> 挑选
    ANDROID_API         minSdk，默认 21（NDK r26+ 要求 >= 21）
    ANDROID_LDFLAGS_EXTRA  追加链接参数，默认 -llog

  macOS 专用环境变量:
    MACOSX_DEPLOYMENT_TARGET   最低系统版本，默认 11.0
    SDKROOT                    macOS SDK；非 macOS 宿主交叉构建时必填

  除上面明确标注"不支持"的组合外，其余组合是否存在可用工具链由本机环境决定，
  脚本会在探测不到 C 编译器时列出已尝试的名字并退出。

常用:
  scripts/build.sh --list
  scripts/build.sh --arch arm64
  scripts/build.sh --os android --arch all
  scripts/build.sh --os macos --arch arm64 SDKROOT=/path/to/MacOSX.sdk
EOF
}

# --os 与 --arch 需要在这里吃掉一部分：
#   --os    只被本脚本消费，不转发（build-<os>.sh 不认识它）。
#   --arch  转发给 build-<os>.sh，但本脚本也要读它，以决定是否再从
#           TARGET_ARCH 取默认值（两者同时给时不能让默认值重复下发）。
declare -a FORWARD=()
OS_FROM_FLAG=""
ARCH_FROM_FLAG=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --list) print_targets; exit 0 ;;
    -l)     print_targets; exit 0 ;;
    --os)      need_value "$1" "${2-}"; OS_FROM_FLAG="$2"; shift 2 ;;
    --os=*)    OS_FROM_FLAG="${1#*=}"; shift ;;
    --arch)    need_value "$1" "${2-}"; ARCH_FROM_FLAG="$2"; shift 2 ;;
    --arch=*)  ARCH_FROM_FLAG="${1#*=}"; shift ;;
    --help) usage; exit 0 ;;
    -h)     usage; exit 0 ;;
    *) FORWARD+=("$1"); shift ;;
  esac
done

if [[ -n "$OS_FROM_FLAG" ]]; then
  if [[ "${TARGET_OS:-auto}" != "auto" && "${TARGET_OS}" != "$(printf '%s' "$TARGET_OS" | tr 'A-Z' 'a-z')" ]]; then
    warn "--os=$OS_FROM_FLAG 与 TARGET_OS=$TARGET_OS 冲突，采用 --os"
  fi
  TARGET_OS="$OS_FROM_FLAG"
fi

# TARGET_ARCH 兼容：--arch 优先，否则用 TARGET_ARCH
ARCH="$(resolve_legacy_arch "$ARCH_FROM_FLAG")"

set -- ${FORWARD[@]+"${FORWARD[@]}"}

case "${TARGET_OS:-auto}" in
  auto|"") TARGET_OS="$(uname -s)" ;;
esac
# uname -s 返回的是 "Linux"/"Darwin"，必须归一化，
# 否则默认路径会掉进"未知 TARGET_OS"分支。
TARGET_OS="$(printf '%s' "$TARGET_OS" | tr 'A-Z' 'a-z')"

case "$TARGET_OS" in
  darwin|macos|osx) OS=macos ;;
  linux)            OS=linux ;;
  windows|win|mingw*|msys*|cygwin*) OS=windows ;;
  android)          OS=android ;;
  *)
    # 与 _common.sh 的 TARGET_OS 保持一致：拼错的给 "你是不是想写"
    suggest "$TARGET_OS" linux macos windows android auto osx win darwin
    die "未知 --os/TARGET_OS: $TARGET_OS（可选: linux macos windows android auto；用 --list 查看完整矩阵）${SUGGEST:+；你是不是想写: $SUGGEST}" ;;
esac

# --arch 只在用户没有自己指定时才从 TARGET_ARCH 转发；
# 否则 TARGET_ARCH 与命令行 --arch 会各传一遍，导致构建出重复/意外的 ABI。
want_arch=""
if [[ "$ARCH" != "auto" ]]; then
  want_arch="$ARCH"
fi

printf '==> %s (宿主 %s, arch=%s)\n' "$OS" "$(uname -s)" "${want_arch:-auto}"
exec "$HERE/build-$OS.sh" ${want_arch:+--arch "$want_arch"} "$@"