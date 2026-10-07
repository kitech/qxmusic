#!/usr/bin/env bash
#
# build-android.sh - 构建 Android 的 libncm.so
#
# 产物:
#   out/android/<abi>/libncm.so
#
# ABI 与 --arch 取值的对应:
#   --arch arm64 -> arm64-v8a   (64 位)
#   --arch armv7 -> armeabi-v7a (32 位，GOARM=7 / VFPv3 硬浮点)
#   --arch amd64 -> x86_64      (模拟器)
#   --arch 386   -> x86         (老模拟器)
#   --arch all   -> 以上全部（推荐，便于打进一个 AAR）
#   --arch 可重复，多个 ABI 一次构建。
#
# 依赖:
#   Android NDK r21+，且 minSdk >= 21（NDK r26+ 强制要求）。
#   NDK 通过 ANDROID_NDK_ROOT 定位；未设置时从 ANDROID_SDK_ROOT/ndk/<最高版本> 自动挑选。
#
# 与桌面端的差别（这些不是可选项，必须对上，否则链接或运行期才炸）:
#   - CC 必须是 NDK 的 llvm 包装器 <triple><api>-clang，apex 风格的裸
#     aarch64-linux-android-gcc 已从 NDK r23 移除，不要再找。
#   - 需要 -llog：__android_log_* 在 NDK 的 liblog.so 里，libandroid.so 不提供。
#   - GOOS=android 只在 CGO_ENABLED=1 下可用。
#
# 环境变量:
#   ANDROID_NDK_ROOT              NDK 路径（规范名）
#   ANDROID_SDK_ROOT              SDK 路径，用于自动挑选 NDK
#   ANDROID_API                   minSdk，默认 21
#   ANDROID_LDFLAGS_EXTRA         追加的链接参数，默认 -llog
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/_common.sh
source "$ROOT/scripts/_common.sh"
cd "$ROOT"

flag "--arch <arch>" "ABI，可重复；all 表示全部。取值: arm64 armv7 amd64 386 all"
flag "--out <dir>" "输出根目录（默认 out）"
flag "--debug" "不剥离符号与调试信息（默认 release）"
flag "-h, --help" "显示本帮助"

OUT_DIR="out"
BUILD_MODE="release"
ANDROID_API="${ANDROID_API:-21}"
ABIS=()
EXTRA_ARGS=()

normalize_args "$@"
[[ ${#NORMALIZED[@]} -gt 0 ]] && set -- "${NORMALIZED[@]}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) BUILD_MODE="debug"; shift ;;
    --out)   need_value "$1" "${2-}"; OUT_DIR="$2"; shift 2 ;;
    --arch)
      need_value "$1" "${2-}"
      if [[ "$2" == "all" ]]; then
        # 追加而非覆盖：与其它 --arch 混用时也能得到全部 4 个 ABI
        ABIS+=(arm64 armv7 amd64 386)
      else
        IFS=',' read -r -a _a <<< "$2"
        ABIS+=(${_a[@]+"${_a[@]}"})
      fi
      shift 2 ;;
    --help) usage; exit 0 ;;
    -h)     usage; exit 0 ;;
    --) shift; EXTRA_ARGS+=("$@"); break ;;
    -?*)
      [[ "$1" == --* ]] && unknown_flag_error "$(basename "$0")" "$1"
      EXTRA_ARGS+=("$1"); shift ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

# android 永远是交叉构建，"auto" 会误取宿主架构（如在 x86 上得到 x86_64），
# 与"给 Android 出库"的意图不符。这里当作未指定。
for i in "${!ABIS[@]}"; do
  [[ "${ABIS[$i]}" == "auto" ]] && unset 'ABIS[i]'
done
ABIS=(${ABIS[@]+"${ABIS[@]}"})
[[ ${#ABIS[@]} -gt 0 ]] || ABIS=(arm64 armv7)
export ANDROID_API

# 同一个 ABI 出现两次会白构建一遍（例如 --arch all --arch armv7，
# 或 TARGET_ARCH 与 --arch 同时指向一个值）。按首次出现去重并保持顺序。
declare -a _uniq=()
for a in ${ABIS[@]+"${ABIS[@]}"}; do
  _seen=0
  for b in ${_uniq[@]+"${_uniq[@]}"}; do [[ "$b" == "$a" ]] && { _seen=1; break; }; done
  [[ "$_seen" == "1" ]] || _uniq+=("$a")
done
ABIS=(${_uniq[@]+"${_uniq[@]}"})

check_go >/dev/null
ensure_go_sum

# 在进入循环前解析一次：find_ndk 里有废弃变量的告警，逐 ABI 调用会重复刷屏。
resolve_ndk_once || die "未找到 NDK。请设置 ANDROID_NDK_ROOT，或设置 ANDROID_SDK_ROOT 让本脚本自动挑选"

LDFLAGS_EXTRA=("-s" "-w")
[[ "$BUILD_MODE" == "release" ]] || LDFLAGS_EXTRA=()

FAILED=()
DONE=()

# 循环里会重置 CC= 再走 NDK 探测。若用户显式预设了 CC，不提示就会被静默忽略，
# 等于"我明明指定了编译器为什么没用"。先说清楚。
[[ -n "${CC:-}" ]] && warn "已忽略外部预设的 CC='$CC'：Android 必须使用 NDK 的 <triple><api>-clang 包装器，请用 ANDROID_API 指定 minSdk。"

for abi in "${ABIS[@]}"; do
  GOARM=""; CC=""
  # Android 一定是交叉构建（宿主是 macOS/Linux，绝不会是 android/*），
  # 这里显式置 1，让 print_target 如实标注，也避免被宿主架构误导。
  CROSS=1
  printf '\n'
  info "===== ABI: $abi ====="

  resolve_arch android "$abi"
  target_supported || { FAILED+=("$abi"); continue; }
  pick_cc
  print_target

  # Go 的 android 目标要求 sysroot 指向 NDK 的统一头文件库
  # （NDK r18+ 已移除独立 gcc sysroot）
  [[ -n "$NDK_SYSROOT" && -d "$NDK_SYSROOT" ]] || die "NDK sysroot 不存在: ${NDK_SYSROOT:-<未探测到>}"

  ABIDIR="$OUT_DIR/android/$(android_abi_dir "$GOARCH")"
  mkdir -p "$ABIDIR/include"

  info "构建 $ABIDIR/libncm.so"
  if ! env CGO_ENABLED=1 \
          GOOS=android \
          GOARCH="$GOARCH" \
          ${GOARM:+GOARM=$GOARM} \
          CC="$CC" \
          CGO_CFLAGS="${CGO_CFLAGS:-} -fPIC -D__ANDROID_API__=$ANDROID_API" \
          CGO_LDFLAGS="${CGO_LDFLAGS:-} $ANDROID_LDFLAGS_EXTRA" \
          go build \
            -buildmode=c-shared \
            -trimpath \
            -ldflags "${LDFLAGS_EXTRA[*]}" \
            -o "$ABIDIR/libncm.so" \
            ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
            ./ncmffi; then
    FAILED+=("$abi")
    continue
  fi

  # 与桌面端一致：必须真的产出库，否则下面的"完成"是假的
  if [[ ! -s "$ABIDIR/libncm.so" ]]; then
    FAILED+=("$abi")
    rm -f "$ABIDIR/libncm.so"
    printf '\033[31m    构建结束但没有产出 libncm.so\033[0m\n' >&2
    continue
  fi

  cp -f include/ncm.h include/ncm.hpp "$ABIDIR/include/"
  # 消费方（jniLibs 的 C/C++ 代码）需要 cgo 生成的 libncm.h 才能拿到
  # ncm_init/ncm_free 等真正的导出声明，否则只能靠手写 extern "C"。
  [[ -f "$ABIDIR/libncm.h" ]] && cp -f "$ABIDIR/libncm.h" "$ABIDIR/include/libncm.generated.h"

  DONE+=("$abi")
  printf '    完成: %s (%s)\n' "$ABIDIR/libncm.so" "$(file_size "$ABIDIR/libncm.so")"
done

printf '\n==> 结果\n'
for abi in "${ABIS[@]}"; do
  dir="$OUT_DIR/android/$(android_abi_dir "$(arch_to_goarch "$abi")")"
  if printf '%s\n' "${DONE[@]+"${DONE[@]}"}" | grep -qx "$abi"; then
    printf '    \033[32m✓\033[0m %-10s %s\n' "$abi" "$dir/libncm.so"
  else
    printf '    \033[31m✗\033[0m %-10s %s\n' "$abi" "<构建失败>"
  fi
done

if [[ ${#FAILED[@]} -gt 0 ]]; then
  printf '\033[31m失败: %s\033[0m\n' "${FAILED[*]}" >&2
  exit 1
fi

cat <<'EOF'

集成到 Android 工程:
  把各 ABI 的 libncm.so 放到
    app/src/main/jniLibs/armeabi-v7a/libncm.so
    app/src/main/jniLibs/arm64-v8a/libncm.so
  消费方（C/C++）在 CMakeLists.txt 里:
    add_library(ncm SHARED IMPORTED)
    set_target_properties(ncm PROPERTIES
      IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libncm.so)
    target_link_libraries(your_app ncm log)   # log 即 NDK 的 -llog

注意: C++ 冒烟测试（examples/）只在桌面端运行，Android 需在设备上验证。
EOF