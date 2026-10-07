#!/usr/bin/env bash
#
# _common.sh - 由 build-*.sh source 的共享逻辑（不是可执行脚本）
#
# 负责：GOOS/GOARCH/GOARM 解析、交叉 C 编译器选择、NDK 探测、前置检查。
# 各平台的差异只应体现在各自 build-*.sh 里。

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
warn() { printf '\033[33mwarn:\033[0m %s\n' "$*" >&2; }
info() { printf '==> %s\n' "$*"; }

# 在 pick_android_cc 运行前也必须存在。
#
# pick_cc 在 CC 已被预设时会提前返回、从而不走 NDK 探测分支；若这里不先
# 赋空值，build-android.sh 里的 [[ -d "$NDK_SYSROOT" ]] 会在 set -u 下
# 报 unbound variable，而不是给出可读的错误。
NDK_SYSROOT=""
ANDROID_LDFLAGS_EXTRA=""

# ---- flag 注册表 ----
#
# 每个脚本用 flag 声明自己支持哪些参数，usage() 依此自动列出。
# 之前 --out / --static 没出现在 --help 里，就是因为参数解析（case 语句）
# 和文档（文件头注释）是两处手工维护的地方 —— 加了参数忘了写文档，
# 没有任何机制会发现。现在声明是唯一真相，check-cli.sh 负责核对。
declare -a FLAGS=()
declare -a FLAG_DESC=()
flag() { FLAGS+=("$1"); FLAG_DESC+=("${2-}"); }

# flag_declared <name>
#
# FLAGS 元素形如 "--arch <arch>"，所以要前缀匹配：既匹配 "--arch"
# 也匹配 "--arch <arch>"，不能全等比较。
flag_declared() {
  local f
  for f in ${FLAGS[@]+"${FLAGS[@]}"}; do
    [[ "$f" == "$1" || "$f" == "$1 "* ]] && return 0
  done
  return 1
}

# flag_names - 只列参数名，供错误信息使用（不带描述，避免刷屏）
flag_names() {
  local out="" f
  for f in ${FLAGS[@]+"${FLAGS[@]}"}; do out+="$f "; done
  printf '%s' "${out% }"
}

usage() {
  local f="${1:-$0}" i
  awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' "$f"
  if [[ ${#FLAGS[@]} -gt 0 ]]; then
    printf '\n参数:\n'
    for ((i = 0; i < ${#FLAGS[@]}; i++)); do
      printf '  %-30s %s\n' "${FLAGS[i]}" "${FLAG_DESC[i]}"
    done
  fi
}

# normalize_args <args...> -> NORMALIZED
#
# 把 --arch=arm64 归一化为 --arch arm64。只拆分已用 flag() 声明过的名字，
# 这样 "--ldflags=-s -w" 这类 go build 参数不会被误拆成两个参数。
normalize_args() {
  local -a out=()
  local a name
  for a in "$@"; do
    name=""
    case "$a" in
      --*=*) name="${a%%=*}" ;;
    esac
    if [[ -n "$name" ]] && flag_declared "$name"; then
      out+=("$name" "${a#*=}")
    else
      out+=("$a")
    fi
  done
  NORMALIZED=(${out[@]+"${out[@]}"})
}

# unknown_flag_error <script> <flag>
#
# 双横线参数里，未声明的一律报错：拼错 flag 若原样透传给 go build，
# 用户会看到一个完全指不到真因的报错。单横线（-ldflags/-gcflags/-tags）
# 才放行 —— 那是 go 自己的参数风格。
unknown_flag_error() {
  local script="$1" bad="$2"
  printf '\033[31merror:\033[0m 未知参数 %s\n' "$bad" >&2
  printf '  %s 支持: %s\n' "$script" "$(flag_names)" >&2
  printf '  若确需透传给 go build，请显式分隔: %s ... -- %s\n' \
    "$script" "$bad" >&2
  exit 1
}

# need_value <flag> <value> - 取 flag 的值，缺值时报错。
#
# 少了这个检查，`--arch --debug` 会把 --debug 当成架构名，
# 错误信息变成"未知 TARGET_ARCH: --debug"，完全看不出是漏了参数。
need_value() {
  if [[ $# -lt 2 || -z "$2" || "$2" == --* ]]; then
    die "参数 $1 缺少值（写法: $1 <值>）"
  fi
}

# ---- 取值纠错 ----
#
# os/arch 的合法取值就那几个，但 arm64/aarch64、armv7/armhf、macos/darwin
# 这些同义名和近拼写极易记混。与其每次都报一个干巴巴的 "未知 xxx"，
# 不如直接告诉用户"是不是想写这个"。
#
# str_dist <a> <b> -> RDIST（编辑距离）。
#   短 token 用完整 DP（O(m*n)）；对 8 字符以内的值没压力，不需要 cutoff。
#   只用 bash 3.2 语法（macOS 自带 bash 不是 4+，避免 ${var,,} 等特性）。
str_dist() {
  local a="$1" b="$2"
  local i j m n
  m=${#a}; n=${#b}
  local -a prev=() cur=()
  for ((j = 0; j <= n; j++)); do prev[j]=$j; done
  for ((i = 1; i <= m; i++)); do
    cur[0]=$i
    for ((j = 1; j <= n; j++)); do
      local cost=0
      [[ "${a:i-1:1}" != "${b:j-1:1}" ]] && cost=1
      local d1=$((prev[j] + 1))      # 删除
      local d2=$((cur[j - 1] + 1))   # 插入
      local d3=$((prev[j - 1] + cost)) # 替换
      local dij=$d1
      (( d2 <= dij )) && dij=$d2
      (( d3 <= dij )) && dij=$d3
      cur[j]=$dij
    done
    prev=("${cur[@]}")
  done
  RDIST=${prev[n]}
}

# suggest <given> <candidates...> -> SUGGEST（"a, b"，最多 2 条，按近度排序）
#
# 距离 <= 2 才纳入；同族再 -1 分（arm7 应优先指向 armv7 而不是距离更远的
# arm64；都含 64/386 同理）。用 v1/v2 两级插入保持有序，避免候选顺序干扰。
suggest() {
  local given="$1"; shift
  local c r
  local v1=99 v2=99 s1="" s2=""
  for c in "$@"; do
    str_dist "$given" "$c"; r=$RDIST
    # 同族候选减近距离（arm7 应优先 armv7 而非 arm64）。
    # 用 r=$((r-1)) 而不是 ((r--))：后者在 r=0 时返回非零，会触发 set -e。
    if   [[ "$given" == arm* && "$c" == arm* ]]; then r=$((r-1))
    elif [[ "$given" == *64* && "$c" == *64* ]]; then r=$((r-1))
    elif [[ "$given" == *386* && "$c" == *386* ]]; then r=$((r-1))
    fi
    if (( r > 2 )); then continue; fi
    if (( r < v1 )); then v2=$v1; s2=$s1; v1=$r; s1=$c; continue; fi
    if (( r < v2 )); then v2=$r; s2=$c; fi
  done
  SUGGEST="$s1"
  # 用 if 而不是 "A && B" 收尾：s2 为空时整个表达式返回非零，
  # 函数被调用处（set -e 的 case 分支）会直接退出且无任何报错。
  if [[ -n "$s2" ]]; then SUGGEST="$s1, $s2"; fi
}

# ---- 依赖完整性 ----

# ensure_go_sum - 缺 go.sum 时补齐。
#
# 这曾是 --tidy flag，但它是一次性 bootstrap 而非构建参数，且默认不开 ——
# 结果是每个新克隆的仓库首次构建都失败，并要求用户猜到那个 flag 再跑一遍。
#
# CI 上只报错、不自动执行：构建脚本隐式改写 go.mod/go.sum 会让依赖漂移
# 难以排查（"本地能过、CI 不能"却看不出是谁改的）。
ensure_go_sum() {
  [[ -f go.sum ]] && return 0

  if [[ -n "${CI:-}" ]]; then
    die "缺少 go.sum。CI 环境不自动执行 go mod tidy，以免隐式改写依赖文件。
  请在本地运行一次并提交 go.sum:
      go mod tidy"
  fi

  info "缺少 go.sum，自动执行 go mod tidy（首次构建需要网络）"
  go mod tidy || die "go mod tidy 失败（通常是网络不可达或依赖冲突）。
  手动运行以查看完整输出:
      go mod tidy"
  [[ -f go.sum ]] || die "go mod tidy 已执行但仍未生成 go.sum，请检查 go.mod 是否声明了依赖"
}

# ---- 废弃的环境变量别名 ----

# first_set <规范名> <废弃名...> -> REPLY
#
# 规范名只保留一个，其余作为已废弃别名继续读取但告警。
# 直接删会让现有 CI 静默改用别处的 NDK（甚至找不到），而降级只花一行 warn。
first_set() {
  local canon="$1"; shift
  REPLY="${!canon:-}"
  local name
  for name in "$@"; do
    [[ -z "${!name:-}" ]] && continue
    if [[ -z "$REPLY" ]]; then
      REPLY="${!name}"
      warn "$name 已废弃，请改用 $canon（本次仍生效）"
    else
      warn "$name 已废弃且被忽略：$canon 已设置"
    fi
  done
}

# resolve_legacy_arch <命令行给的 arch> -> 最终 arch
#
# TARGET_ARCH 已被 --arch 取代，但保留读取（并告警）：现有 CI 里大量
# 脚本用的是 TARGET_ARCH=arm64 build.sh，直接删会让它们静默退回宿主架构。
# 变量只在这里出现，别处不再直接引用。
resolve_legacy_arch() {
  local cli="$1"
  if [[ -n "$cli" ]]; then
    if [[ -n "${TARGET_ARCH:-}" && "${TARGET_ARCH}" != "auto" ]]; then
      warn "TARGET_ARCH 已废弃且被忽略：--arch 已指定（本次 --arch 生效）"
    fi
    printf '%s' "$cli"
    return 0
  fi
  if [[ -n "${TARGET_ARCH:-}" && "${TARGET_ARCH}" != "auto" ]]; then
    warn "TARGET_ARCH 已废弃，请改用 --arch（本次仍生效）"
    printf '%s' "$TARGET_ARCH"
    return 0
  fi
  printf 'auto'
}

# c-shared 的"确定不支持"组合。
#
# 这里只拒绝确定无效的组合，不去维护一张"全部支持"白名单：
# 白名单一漏就会误杀合法目标（如曾漏掉 linux/386），而漏掉的代价是
# 用户拿到一条难懂的链接错误 —— 后者明显更好。
# 其余组合是否真的可用，交给 go build 与 C 工具链裁决。
target_supported() {
  case "$GOOS/$GOARCH" in
    darwin/arm)
      warn "darwin/arm 不是合法的 GOOS/GOARCH 组合（Go 不提供 32 位 macOS）。"
      warn "Apple Silicon 请用 --arch arm64。"
      return 1 ;;
    js/*|wasip1/*|plan9/*)
      warn "$GOOS/$GOARCH 不支持 cgo，无法产出 c-shared。"
      return 1 ;;
  esac
  return 0
}

# resolve_arch <TARGET_OS> <TARGET_ARCH>
#   TARGET_ARCH: auto|amd64|arm64|arm|armv5|armv6|armv7(armhf)
# 输出: GOOS GOARCH GOARM ARCH_LABEL CROSS
resolve_arch() {
  local os="$1" arch="$2"

  # 注意：Go 的 GOOS 是 linux/darwin/windows/android，没有 "macos"。
  # 一旦这里写成 macos，go build 会直接报 unsupported GOOS。
  # 大小写不敏感（macOS bash3 无 ${var,,}，用 tr）；os= 同时兼容
  # 同义名 macos/darwin/osx、windows/win/mingw/msys/cygwin。
  os="$(printf '%s' "$os" | tr 'A-Z' 'a-z')"
  case "$os" in
    linux|android)     GOOS="$os";     OS_LABEL="$os" ;;
    macos|darwin|osx)  GOOS=darwin;    OS_LABEL=macos ;;
    windows|win)       GOOS=windows;   OS_LABEL=windows ;;
    mingw|msys|cygwin) GOOS=windows;   OS_LABEL=windows; os=windows ;;
    *)
      suggest "$os" linux macos windows android auto
      die "未知 TARGET_OS: $os（可选: linux macos windows android auto）${SUGGEST:+；你是不是想写: $SUGGEST}" ;;
  esac

  if [[ -z "$arch" || "$arch" == "auto" ]]; then
    warn "TARGET_ARCH 未指定，按宿主 $(uname -m) 处理；交叉编译请显式传 --arch"
    case "$os" in
      windows) arch="$(uname -m | sed -E 's/^x86_64$/amd64/; s/^aarch64$/arm64/')" ;;
      *)       arch="$(uname -m)" ;;
    esac
  fi

  # arch 名大小写不敏感：--arch ARM64 与 --arch arm64 等价。
  # 只在匹配时用小写比较，输出仍保留原样。
  local arch_match; arch_match="$(printf '%s' "$arch" | tr 'A-Z' 'a-z')"
  case "$arch_match" in
    amd64|x86_64|x64)  GOARCH=amd64; ARCH_LABEL=x86_64  ;;
    arm64|aarch64)     GOARCH=arm64; ARCH_LABEL=aarch64 ;;
    arm|armv7|armv7l|armhf) GOARCH=arm; ARCH_LABEL=armv7; GOARM="${GOARM:-7}" ;;
    armv6|armv6l)      GOARCH=arm;   ARCH_LABEL=armv6;  GOARM="${GOARM:-6}" ;;
    armv5|armsf)       GOARCH=arm;   ARCH_LABEL=armv5;  GOARM="${GOARM:-5}" ;;
    386|i386|i686)     GOARCH=386;   ARCH_LABEL=x86     ;;
    *)
      # 常见混淆拼写单独处理：armhf 与 armv7 同义、aarch64 与 arm64 同义已在
      # 上面覆盖；这里只补最常见的误转写（arm32->armv7 等靠 suggest 兜底）。
      suggest "$arch_match" arm64 amd64 armv7 armv6 armv5 386
      die "未知 TARGET_ARCH: $arch（可选: amd64 arm64 armv7 armv6 armv5 386）${SUGGEST:+；你是不是想写: $SUGGEST}" ;;
  esac

  if [[ "$GOARCH" == "arm" ]]; then
    [[ "$GOARM" =~ ^[567]$ ]] || die "GOARM 只能是 5/6/7，当前 '$GOARM'"
    # GOARM 决定浮点 ABI；与工具链不匹配会在运行时报 Illegal instruction，
    # 而不是编译期报错，所以这里提前拦。
    case "$GOARM" in
      5) [[ "$ARCH_LABEL" == "armv5" ]] || die "GOARM=5 需配合 --arch armv5/armsf（软浮点）" ;;
      6) [[ "$ARCH_LABEL" == "armv6" ]] || die "GOARM=6 需配合 --arch armv6（VFPv1）" ;;
      7) [[ "$ARCH_LABEL" == "armv7" ]] || die "GOARM=7 需配合 --arch armv7/armhf（VFPv3）" ;;
    esac
  fi

  case "$GOOS/$GOARCH" in
    darwin/arm)
      die "macOS 不支持 32 位 ARM（darwin/arm 在 Go 中不存在）。Apple Silicon 请用 --arch arm64" ;;
    android/arm)
      if [[ "$GOARM" != "7" ]]; then
        die "NDK r17+ 只提供 armeabi-v7a，GOARM 只能为 7（当前 $GOARM）。" \
            "如确需 armv6/armv5，请使用 NDK r16 及更早版本"
      fi ;;
  esac

  CROSS=0
  local host; host="$(uname -m)"
  case "$GOARCH" in
    amd64) [[ "$host" == "x86_64" || "$host" == "amd64" ]] || CROSS=1 ;;
    arm64) [[ "$host" == "aarch64" || "$host" == "arm64" ]] || CROSS=1 ;;
    arm)   [[ "$host" == "armv7l" || "$host" == "armv6l" ]] || CROSS=1 ;;
    386)   [[ "$host" == "i386" || "$host" == "i686" ]] || CROSS=1 ;;
  esac
  # 宿主 OS 与目标 OS 不同也算交叉（如 Linux 上构建 darwin）
  if [[ "$GOOS" != "$(host_os)" ]]; then
    CROSS=1
  fi
}

host_os() {
  case "$(uname -s)" in
    Darwin) echo darwin ;;
    Linux)  echo linux ;;
    MINGW*|MSYS*|CYGWIN*) echo windows ;;
    *) echo "$(uname -s | tr 'A-Z' 'a-z')" ;;
  esac
}

# ---- ABI 目录名 ----

# arch_to_goarch: 用户传入的 arch 名 -> GOARCH
arch_to_goarch() {
  case "$1" in
    amd64|x86_64)  echo amd64 ;;
    arm64|aarch64) echo arm64 ;;
    armv7|arm|armhf) echo arm ;;
    386|i386)      echo 386 ;;
    *) echo "$1" ;;
  esac
}

# android_abi_dir: GOARCH -> Android ABI 目录名
# 不能用 $GOARCH 直接当目录名：GOARCH=arm 对应的 ABI 名是 armeabi-v7a，
# 直接用 arm 会在 jniLibs 里放错位置，导致 64 位设备加载到 32 位库。
android_abi_dir() {
  case "$1" in
    arm64) echo "arm64-v8a" ;;
    arm)   echo "armeabi-v7a" ;;
    amd64) echo "x86_64" ;;
    386)   echo "x86" ;;
    *) die "无对应的 Android ABI 目录名: $1" ;;
  esac
}

file_size() {
  local b
  b="$(wc -c < "$1" 2>/dev/null || echo 0)"
  if   (( b >= 1048576 )); then printf '%d.%d MiB' $((b/1048576)) $((b%1048576*10/1048576))
  elif (( b >= 1024 ));    then printf '%d.%d KiB' $((b/1024))    $((b%1024*10/1024))
  else printf '%d B' "$b"; fi
}

# ---- NDK ----

# ndk_host_tag: NDK 预编译工具链的宿主目录名
ndk_host_tag() {
  case "$(uname -s)" in
    Darwin)
      case "$(uname -m)" in
        arm64|aarch64) echo "darwin-arm64" ;;   # NDK r26+ 才有官方 darwin-arm64
        *)             echo "darwin-x86_64" ;;
      esac ;;
    Linux)   echo "linux-x86_64" ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows-x86_64" ;;
    *) die "NDK 不支持宿主系统: $(uname -s)" ;;
  esac
}

# ndk_triple: GOARCH -> NDK clang 前缀
ndk_triple() {
  case "$1" in
    arm64) echo "aarch64-linux-android" ;;
    arm)   echo "armv7a-linux-androideabi" ;;  # NDK r17+ 仅 armeabi-v7a
    amd64) echo "x86_64-linux-android" ;;
    386)   echo "i686-linux-android" ;;
    *) die "无对应的 NDK triple: $1" ;;
  esac
}

# find_sdk: 规范名 ANDROID_SDK_ROOT；ANDROID_HOME 为已废弃别名
find_sdk() {
  first_set ANDROID_SDK_ROOT ANDROID_HOME
  [[ -n "$REPLY" ]] || return 1
  printf '%s' "$REPLY"
}

# find_ndk: 规范名 ANDROID_NDK_ROOT，其次从 SDK 的 ndk/<最高版本> 里自动选
find_ndk() {
  first_set ANDROID_NDK_ROOT ANDROID_NDK_HOME NDK_HOME
  local ndk="$REPLY"
  if [[ -z "$ndk" ]]; then
    find_sdk || return 1
    [[ -d "$REPLY/ndk" ]] || return 1
    local v; v="$(ls -1 "$REPLY/ndk" 2>/dev/null | sort -V | tail -1)"
    [[ -n "$v" ]] || return 1
    ndk="$REPLY/ndk/$v"
  fi
  [[ -d "$ndk" ]] || return 1
  printf '%s' "$ndk"
}

# resolve_ndk_once - 在进入 ABI 循环前解析一次并回写规范名。
#
# find_ndk 里有告警，而构建会为每个 ABI 各调一次；放在循环外解析
# 既避免重复告警，也省掉重复的 ls/sort。
resolve_ndk_once() {
  local ndk; ndk="$(find_ndk)" || return 1
  ANDROID_NDK_ROOT="$ndk"
  export ANDROID_NDK_ROOT
  return 0
}

# pick_android_cc: 组装 <triple><api>-clang 并校验存在
pick_android_cc() {
  local api="${ANDROID_API:-21}"
  [[ "$api" =~ ^[0-9]+$ ]] || die "ANDROID_API 必须是数字，当前 '$api'"
  (( api >= 21 )) || die "NDK r26+ 要求 minSdk >= 21，当前 ANDROID_API=$api"

  local ndk="${ANDROID_NDK_ROOT:-}"
  if [[ -z "$ndk" ]]; then
    ndk="$(find_ndk)" \
      || die "未找到 NDK。请设置 ANDROID_NDK_ROOT，或设置 ANDROID_SDK_ROOT 后由本脚本自动挑选 \$ANDROID_SDK_ROOT/ndk/<版本>"
  fi
  local tag; tag="$(ndk_host_tag)"
  local bindir="$ndk/toolchains/llvm/prebuilt/$tag/bin"
  [[ -d "$bindir" ]] || die "$(printf 'NDK 工具链目录不存在: %s\n宿主标签 %s；若为 Apple Silicon 需安装 NDK r26+ 才有官方 darwin-arm64 版本' \
      "$bindir" "$tag")"

  local triple; triple="$(ndk_triple "$GOARCH")"
  local cc="$bindir/$triple${api}-clang"
  if [[ ! -x "$cc" ]]; then
    # NDK 只为有限的 API 级别提供 wrapper（通常只有 21/最新版本）。
    # 报"文件不存在"很容易让人以为是 NDK 装坏了，其实多半只是 API 填大了。
    local avail; avail="$(cd "$bindir" 2>/dev/null && ls "$triple"*-clang 2>/dev/null | tr '\n' ' ')"
    if [[ -n "$avail" ]]; then
      die "找不到 NDK 编译器: $cc
  该 NDK 只提供这些 wrapper: $avail
  请改用其中一个 API 级别，例如: ANDROID_API=21 $0 $*"
    fi
    die "找不到 NDK 编译器: $cc（该 NDK 里连 $triple*-clang 都没有，NDK 安装可能不完整）"
  fi

  CC="$cc"
  NDK_SYSROOT="$ndk/toolchains/llvm/prebuilt/$tag/sysroot"
  ANDROID_API="$api"
  # __android_log_print 这类符号在 NDK 的 liblog.so 里，libandroid.so 并不提供；
  # 少了 -llog 会在链接期报 undefined reference。传一个用不到的 -l 不会有副作用
  # （不像 --no-undefined 会失败），所以这里保守地带上，并允许用
  # ANDROID_LDFLAGS_EXTRA 覆盖以适配特殊工程。
  ANDROID_LDFLAGS_EXTRA="${ANDROID_LDFLAGS_EXTRA:--llog}"
}

# ---- 通用 C 编译器选择（非 Android） ----

pick_cc() {
  if [[ -n "${CC:-}" ]]; then
    command -v "$CC" >/dev/null 2>&1 || die "CC=$CC 不存在"
    # Android 走 NDK clang 包装器，不接受外部随意给的编译器：外部 CC 缺少
    # <triple><api> 前缀，cgo 会拿不到正确的 target/sysroot。明确拒绝，
    # 好过让用户在一堆链接错误里猜原因。
    if [[ "$GOOS" == "android" ]]; then
      die "GOOS=android 不接受外部预设的 CC（当前 CC=$CC）。请用 ANDROID_API 指定 minSdk，让脚本自行定位 NDK。"
    fi
    return
  fi

  if [[ "$GOOS" == "android" ]]; then pick_android_cc; return; fi

  local candidates=()
  case "$GOOS/$GOARCH" in
    linux/amd64)
      if   command -v gcc   >/dev/null 2>&1; then CC=gcc
      elif command -v cc    >/dev/null 2>&1; then CC=cc
      elif command -v clang >/dev/null 2>&1; then CC=clang
      else die "未找到 gcc/cc/clang"; fi
      ;;
    linux/arm64)
      candidates=(aarch64-linux-gnu-gcc aarch64-linux-gnu-gcc-12 aarch64-linux-gnu-gcc-11
                  aarch64-linux-gnu-gcc-10 aarch64-none-linux-gnu-gcc clang) ;;
    linux/arm)
      if [[ "$GOARM" == "5" ]]; then
        candidates=(arm-linux-gnueabi-gcc arm-linux-gnueabi-gcc-12)
      else
        candidates=(arm-linux-gnueabihf-gcc arm-linux-gnueabihf-gcc-12 arm-linux-gnueabihf-gcc-11)
      fi ;;
    linux/386)
      if   command -v gcc          >/dev/null 2>&1; then CC=gcc
      elif command -v i686-linux-gnu-gcc >/dev/null 2>&1; then CC=i686-linux-gnu-gcc
      else die "未找到 32 位 C 编译器（构建 linux/386 需 gcc-multilib）"; fi ;;
    darwin/*)
      command -v clang >/dev/null 2>&1 || die "未找到 clang，请先安装 Xcode Command Line Tools"
      CC=clang ;;
    windows/amd64)
      case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) CC=gcc ;;
        *) candidates=(x86_64-w64-mingw32-gcc x86_64-w64-mingw32-gcc-posix) ;;
      esac ;;
    windows/arm64)
      case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) CC=gcc ;;
        *) candidates=(aarch64-w64-mingw32-gcc aarch64-w64-mingw32-gcc-posix) ;;
      esac ;;
    windows/arm)
      die "windows/arm(32 位) 没有可用的 mingw-w64 工具链，交叉编译不可行。
     若确需 32 位 ARM，请改用 Android 的 --arch armv7 (armeabi-v7a)。" ;;
    *) die "无 C 编译器选择逻辑: $GOOS/$GOARCH" ;;
  esac

  [[ -n "${CC:-}" ]] || {
    for c in ${candidates[@]+"${candidates[@]}"}; do
      command -v "$c" >/dev/null 2>&1 && { CC="$c"; break; }
    done
  }
  [[ -n "${CC:-}" ]] || die "$(printf '未找到 %s/%s 的 C 编译器。已尝试: %s\nDebian/Ubuntu 可安装:\n  sudo apt install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf' \
      "$GOOS" "$GOARCH" "${candidates[*]:-(见上方说明)}")"
}

# clang 交叉到非宿主架构时需要显式 --target，否则会用宿主 ABI
arch_cflags() {
  case "$(basename "${CC:-cc}")" in
    *clang*) ;;
    *) return 0 ;;
  esac
  [[ "$CROSS" == "1" ]] || return 0
  case "$GOOS/$GOARCH" in
    linux/arm64)  printf -- '--target=aarch64-linux-gnu ' ;;
    linux/amd64)  printf -- '--target=x86_64-linux-gnu ' ;;
    linux/arm)    printf -- '--target=armv7a-linux-gnueabihf ' ;;
    linux/386)    printf -- '--target=i386-linux-gnu ' ;;
    darwin/arm64) printf -- '-arch arm64 ' ;;
    darwin/amd64) printf -- '-arch x86_64 ' ;;
    windows/arm64) printf -- '--target=aarch64-w64-windows-gnu ' ;;
    windows/amd64) printf -- '--target=x86_64-w64-windows-gnu ' ;;
  esac
}

check_go() {
  command -v go >/dev/null 2>&1 || die "未找到 go，请先安装 Go（>= 1.22）"
  local v; v="$(go env GOVERSION)"
  info "go: $v"
  case "$v" in
    go1.1[0-9]*|go1.2[01]*) die "需要 Go >= 1.22，当前 $v" ;;
  esac
}

print_target() {
  printf '==> 目标: %s/%s  (GOOS=%s GOARCH=%s GOARM=%s CC=%s%s)\n' \
    "$OS_LABEL" "$ARCH_LABEL" "$GOOS" "$GOARCH" "${GOARM:--}" "${CC:-—}" \
    "$( [[ "$CROSS" == "1" ]] && printf ' [交叉编译]' )"
}