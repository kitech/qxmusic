#!/usr/bin/env bash
#
# build-linux.sh - 构建 Linux 的 libncm.so
#
# 产物:
#   out/linux/<arch>/libncm.so   （--arch 非本机时）
#   out/libncm.so                （--arch 为本机时，便于直接链接）
#   out/include/                 对外稳定头文件 ncm.h / ncm.hpp
#
# 用法:
#   scripts/build-linux.sh                          # 本机架构
#   scripts/build-linux.sh --arch arm64             # 交叉编译 aarch64
#   scripts/build-linux.sh --arch armv7             # 交叉编译 armv7 (GOARM=7)
#   scripts/build-linux.sh --arch armv6 --out dist  # 指定输出目录
#   scripts/build-linux.sh --debug
#
# 依赖:
#   go.sum 缺失时首次构建会自动执行 go mod tidy（CI 环境改为报错，
#   以免隐式改写依赖文件）。
#
# 交叉编译器（Debian/Ubuntu）:
#   sudo apt install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf
#   指定 armv6/GOARM=6 用 gcc-arm-linux-gnueabi；GOARM=5(软浮点) 同理。
#
# 环境变量:
#   CC CGO_CFLAGS CGO_LDFLAGS   透传给 cgo（一般不用设）
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/_common.sh
source "$ROOT/scripts/_common.sh"
cd "$ROOT"

flag "--arch <arch>" "目标架构: amd64 arm64 armv7 armv6 armv5 386（默认本机）"
flag "--out <dir>" "输出根目录（默认 out）"
flag "--debug" "不剥离符号与调试信息（默认 release，产出 -s -w）"
flag "-h, --help" "显示本帮助"

OUT_DIR="out"
BUILD_MODE="release"
ARCH="auto"
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
      # 双横线却不在注册表里 = 拼错了。单横线（-ldflags/-gcflags/-tags）才是 go 的参数。
      [[ "$1" == --* ]] && unknown_flag_error "$(basename "$0")" "$1"
      EXTRA_ARGS+=("$1"); shift ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

check_go >/dev/null
ensure_go_sum

resolve_arch linux "$ARCH"
target_supported || exit 1
pick_cc
print_target

LDFLAGS_EXTRA=()
[[ "$BUILD_MODE" == "release" ]] && LDFLAGS_EXTRA+=("-s" "-w")

# 本机架构直接放 out/ 方便链接；交叉产物放 out/linux/<arch>/ 避免互相覆盖
if [[ "$CROSS" == "1" ]]; then
  DEST="$OUT_DIR/linux/$ARCH_LABEL"
  mkdir -p "$DEST/include"
else
  DEST="$OUT_DIR"
  mkdir -p "$DEST/include"
fi

# 用独立变量承载"本脚本要追加的 flag"，避免覆盖后又被自身重复拼接
EXTRA_CFLAGS="$(arch_cflags)-fPIC -D_FILE_OFFSET_BITS=64"
EXTRA_LDFLAGS=""

info "构建 $DEST/libncm.so"
# 注意：这里必须用 env 而不是 `VAR=x cmd` 前缀赋值 ——
# 在赋值前缀列表里放 ${GOARM:+GOARM=$GOARM} 这类可能展开为空的表达式，
# bash 会终止赋值列表，并把后面的 VAR=x 当成命令去执行（VAR=x: command not found）。
env CGO_ENABLED=1 \
    GOOS=linux \
    GOARCH="$GOARCH" \
    ${GOARM:+GOARM="$GOARM"} \
    CC="$CC" \
    CGO_CFLAGS="${CGO_CFLAGS:-} ${EXTRA_CFLAGS}" \
    CGO_LDFLAGS="${CGO_LDFLAGS:-} ${EXTRA_LDFLAGS}" \
    go build \
      -buildmode=c-shared \
      -trimpath \
      -ldflags "${LDFLAGS_EXTRA[*]}" \
      -o "$DEST/libncm.so" \
      ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
      ./ncmffi

# 产物必须真的存在，否则上面的"完成"是假的（磁盘满、交叉链接静默失败等）
[[ -s "$DEST/libncm.so" ]] || die "构建未产出 libncm.so: $DEST/libncm.so"

cp -f include/ncm.h include/ncm.hpp "$DEST/include/"
[[ -f "$DEST/libncm.h" ]] && cp -f "$DEST/libncm.h" "$DEST/include/libncm.generated.h"

printf '\n==> 完成\n'
printf '    库:     %s/libncm.so (%s)\n' "$DEST" "$(file_size "$DEST/libncm.so")"
printf '    头文件: %s/include/{ncm.h,ncm.hpp}\n' "$DEST"
printf '    链接:   -L%s -lncm -Wl,-rpath,%s\n' "$(cd "$DEST" && pwd)" "$(cd "$DEST" && pwd)"

if command -v nm >/dev/null 2>&1; then
  printf '    ncm_* 导出符号: %s 个\n' \
    "$(nm -D --defined-only "$DEST/libncm.so" 2>/dev/null | grep -c ' T ncm_' || echo 0)"
fi

printf '\n冒烟测试: make -C examples\n'