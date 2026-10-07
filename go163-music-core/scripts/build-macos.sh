#!/usr/bin/env bash
#
# build-macos.sh - 构建 macOS 的 libncm.dylib
#
# 用法:
#   scripts/build-macos.sh                       # 本机架构（Apple Silicon = arm64）
#   scripts/build-macos.sh --arch amd64          # Intel
#   scripts/build-macos.sh --arch arm64 SDKROOT=/path/to/MacOSX.sdk
#   MACOSX_DEPLOYMENT_TARGET=12.0 scripts/build-macos.sh
#
# 不支持 32 位 ARM: darwin/arm 在 Go 中不存在，Apple Silicon 只有 arm64。
#
# 在 Linux 上交叉构建需要一份 macOS SDK:
#   export SDKROOT=/path/to/MacOSX.sdk
#   scripts/build.sh --os macos --arch arm64
# go build 没有 -sysroot 选项，SDK 路径只能经 CGO_CFLAGS/CGO_LDFLAGS 传给 clang。
#
# 环境变量:
#   MACOSX_DEPLOYMENT_TARGET   最低系统版本（默认 11.0）
#   SDKROOT                    macOS SDK 路径；非 macOS 宿主交叉构建时必填
#   CC CGO_CFLAGS CGO_LDFLAGS   透传给 cgo（一般不用设）
#
# UI 侧链接（未安装 dylib 到系统目录时需允许未解析符号）:
#   clang++ app.cpp -o app -Iout/include -Lout -lncm \
#            -Wl,-rpath,@executable_path/out -Wl,-undefined,dynamic_lookup
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/_common.sh
source "$ROOT/scripts/_common.sh"
cd "$ROOT"

flag "--arch <arch>" "目标架构: arm64 amd64（本机）"
flag "--out <dir>" "输出根目录（默认 out）"
flag "--debug" "不剥离符号与调试信息（默认 release）"
flag "-h, --help" "显示本帮助"

OUT_DIR="out"
BUILD_MODE="release"
ARCH="auto"
DEPLOY_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}"
EXTRA_ARGS=()

normalize_args "$@"
[[ ${#NORMALIZED[@]} -gt 0 ]] && set -- "${NORMALIZED[@]}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) BUILD_MODE="debug"; shift ;;
    --out)   need_value "$1" "${2-}"; OUT_DIR="$2"; shift 2 ;;
    --arch)  need_value "$1" "${2-}"; ARCH="$2"; shift 2 ;;
    --help) usage; exit 0 ;;
    -h)     usage; exit 0 ;;
    --) shift; EXTRA_ARGS+=("$@"); break ;;
    -?*)
      [[ "$1" == --* ]] && unknown_flag_error "$(basename "$0")" "$1"
      EXTRA_ARGS+=("$1"); shift ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

check_go >/dev/null
ensure_go_sum

resolve_arch macos "$ARCH"     # darwin/arm 会在此被拒绝
target_supported || exit 1
pick_cc                        # macOS 一律 clang
print_target

if [[ "$(uname -s)" != "Darwin" && -z "${SDKROOT:-}" ]]; then
  die "在非 macOS 上交叉构建必须显式指定 SDKROOT（macOS SDK 路径）"
fi

MINFLAG="-mmacosx-version-min=$DEPLOY_TARGET"
EXTRA_CFLAGS="$(arch_cflags)-fPIC $MINFLAG"
EXTRA_LDFLAGS="$MINFLAG"
if [[ -n "${SDKROOT:-}" ]]; then
  [[ -d "$SDKROOT" ]] || die "SDKROOT 不存在: $SDKROOT"
  EXTRA_CFLAGS+=" -isysroot $SDKROOT"
  EXTRA_LDFLAGS+=" -isysroot $SDKROOT"
fi

LDFLAGS_EXTRA=("-s" "-w")
[[ "$BUILD_MODE" == "release" ]] || LDFLAGS_EXTRA=()

if [[ "$CROSS" == "1" ]]; then
  DEST="$OUT_DIR/macos/$ARCH_LABEL"
else
  DEST="$OUT_DIR"
fi
mkdir -p "$DEST/include"

info "构建 $DEST/libncm.dylib"
env CGO_ENABLED=1 \
    GOOS=darwin \
    GOARCH="$GOARCH" \
    CC="$CC" \
    CGO_CFLAGS="${CGO_CFLAGS:-} ${EXTRA_CFLAGS}" \
    CGO_LDFLAGS="${CGO_LDFLAGS:-} ${EXTRA_LDFLAGS}" \
    go build \
      -buildmode=c-shared \
      -trimpath \
      -ldflags "${LDFLAGS_EXTRA[*]}" \
      -o "$DEST/libncm.dylib" \
      ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
      ./ncmffi

[[ -s "$DEST/libncm.dylib" ]] || die "构建未产出 libncm.dylib: $DEST/libncm.dylib"

cp -f include/ncm.h include/ncm.hpp "$DEST/include/"
[[ -f "$DEST/libncm.h" ]] && cp -f "$DEST/libncm.h" "$DEST/include/libncm.generated.h"

printf '\n==> 完成\n'
printf '    库:     %s/libncm.dylib (%s)\n' "$DEST" "$(file_size "$DEST/libncm.dylib")"
printf '    头文件: %s/include/{ncm.h,ncm.hpp}\n' "$DEST"
if command -v otool >/dev/null 2>&1; then
  printf '    arch:   %s\n' "$(lipo -archs "$DEST/libncm.dylib" 2>/dev/null || echo '?')"
  printf '    min:    %s\n' "$(otool -l "$DEST/libncm.dylib" 2>/dev/null \
      | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')"
fi

printf '\n冒烟测试: make -C examples\n'