#!/usr/bin/env bash
#
# build-windows.sh - 构建 Windows 的 ncm.dll
#
# 用法:
#   scripts/build-windows.sh                # 本机（MSYS2/MinGW 环境）
#   scripts/build-windows.sh --arch arm64   # 交叉构建 aarch64
#   scripts/build-windows.sh --static       # 改为产出 ncm.a（c-archive）
#
# 工具链:
#   - Windows 原生: MSYS2 的 mingw-w64（pacman -S mingw-w64-x86_64-gcc）
#   - Linux 交叉:   sudo apt install mingw-w64
#     arm64 需较新的 mingw-w64（提供 aarch64-w64-mingw32-gcc）；
#     只有 -posix 变体支持线程/异常，C++ 消费方务必选 -posix。
#
# 产物:
#   -buildmode=c-shared -> ncm.dll（需 .lib 导入库时用 dlltool 生成）
#   -buildmode=c-archive -> ncm.a（不产出 dll，两者需分别构建）
#
# 环境变量:
#   CC CGO_CFLAGS CGO_LDFLAGS   透传给 cgo（一般不用设）
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/_common.sh
source "$ROOT/scripts/_common.sh"
cd "$ROOT"

flag "--arch <arch>" "目标架构: amd64 arm64（本机）"
flag "--static" "产出 ncm.a 而非 ncm.dll（-buildmode=c-archive）"
flag "--out <dir>" "输出根目录（默认 out）"
flag "--debug" "不剥离符号与调试信息（默认 release）"
flag "-h, --help" "显示本帮助"

OUT_DIR="out"
BUILD_MODE="release"
ARCH="auto"
MODE="shared"
EXTRA_ARGS=()

normalize_args "$@"
[[ ${#NORMALIZED[@]} -gt 0 ]] && set -- "${NORMALIZED[@]}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)  BUILD_MODE="debug"; shift ;;
    --out)    need_value "$1" "${2-}"; OUT_DIR="$2"; shift 2 ;;
    --arch)   need_value "$1" "${2-}"; ARCH="$2"; shift 2 ;;
    --static) MODE="archive"; shift ;;
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

resolve_arch windows "$ARCH"   # windows/arm 不在 c-shared 支持列表内
target_supported || exit 1
pick_cc
print_target

if [[ "$CROSS" == "1" ]]; then
  DEST="$OUT_DIR/windows/$ARCH_LABEL"
else
  DEST="$OUT_DIR"
fi
mkdir -p "$DEST/include"

LDFLAGS_EXTRA=()
[[ "$BUILD_MODE" == "release" ]] && LDFLAGS_EXTRA+=("-s" "-w")

case "$MODE" in
  shared)  BMODE=c-shared;  OUTFILE="ncm.dll"; ART="ncm.dll" ;;
  archive) BMODE=c-archive; OUTFILE="ncm.a";   ART="ncm.a" ;;
esac

info "构建 $DEST/$OUTFILE (-buildmode=$BMODE)"
env CGO_ENABLED=1 \
    GOOS=windows \
    GOARCH="$GOARCH" \
    ${GOARM:+GOARM="$GOARM"} \
    CC="$CC" \
    CGO_CFLAGS="${CGO_CFLAGS:-} $(arch_cflags)" \
    go build \
      -buildmode="$BMODE" \
      -trimpath \
      -ldflags "${LDFLAGS_EXTRA[*]}" \
      -o "$DEST/$OUTFILE" \
      ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
      ./ncmffi

[[ -s "$DEST/$OUTFILE" ]] || die "构建未产出 $OUTFILE: $DEST/$OUTFILE"

cp -f include/ncm.h include/ncm.hpp "$DEST/include/"
[[ -f "$DEST/ncm.h" ]] && cp -f "$DEST/ncm.h" "$DEST/include/libncm.generated.h"

printf '\n==> 完成\n'
printf '    库:     %s/%s (%s)\n' "$DEST" "$ART" "$(file_size "$DEST/$OUTFILE")"
printf '    头文件: %s/include/{ncm.h,ncm.hpp}\n' "$DEST"

cat <<'EOF'

UI 侧链接示例（消费方不定义 NCM_BUILDING_SHARED，ncm.h 会自动切 dllimport）:
  g++ app.cpp -o app.exe -Iout/include out/ncm.dll

需要 .lib 导入库时:
  dlltool -d ncm.dll -D ncm.lib -D ncm.dll

冒烟测试（MSYS2 下需用 UCRT64/MINGW64 shell，否则 ABI 与 MSVC 不兼容）:
  make -C examples
EOF