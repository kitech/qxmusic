#!/usr/bin/env bash
#
# check-cli.sh - 核对构建脚本"参数声明 ↔ 解析 ↔ 文档"三者一致。
#
# 背景：--out 和 --static 曾因参数解析与文档分头手工维护而漏出文档。
# 本脚本把这条检查变成可执行的，避免再次发生：
#   1. flag() 声明了但解析器没处理  -> 用户传了也白传
#   2. flag() 声明了但 --help 查不到 -> 你不知道它存在
#   3. 脚本注释里提到但已不存在的 flag -> 误导
#   4. 已废弃的 flag 重新出现       -> 旧参数悄悄复活，多通道输入又回来
#
# 纯文本/A 端到端 —help 比对，不编译。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
fail=0

BUILD_SCRIPTS=(scripts/build.sh scripts/build-linux.sh scripts/build-macos.sh
               scripts/build-windows.sh scripts/build-android.sh)

# 已彻底移除的 flag：解析分支里再出现就是回归
REMOVED=(--release --tidy --min-macos --api --abi)

# 声明：flag "...格式..." 行中的长参数名
declared_flags() {
  grep -oE '^flag "[^"]+"' "$1" |
    sed -E 's/^flag "//; s/"$//' |
    awk '{print $1}' | tr -d ',' | grep -E '^--' | sort -u
}

# 解析：case 分支里出现的参数名（含 --arch= 这类带尾缀的）
parsed_flags() {
  grep -oE '^[[:space:]]+(-{1,2}[a-zA-Z][a-zA-Z0-9-]*)(=[^)]*)?\)' "$1" |
    sed -E 's/[[:space:]]+//; s/\)$//' |
    sort -u
}

# 文档：文件头注释里出现的 --flags；跳过引用 build.sh 的示例行
documented_flags() {
  awk 'NR>1 && /^#/ { print; next } NR>1 { exit }' "$1" |
    grep -v 'scripts/build\.sh' |
    grep -oE '\-\-[a-zA-Z][a-zA-Z0-9-]*' | sort -u
}

echo "== 参数一致性核对 =="
for f in "${BUILD_SCRIPTS[@]}"; do
  printf '\n%s\n' "$f"
  d=$(declared_flags "$f")
  p=$(parsed_flags "$f")
  c=$(documented_flags "$f")

  while read -r name; do
    [[ -z "$name" ]] && continue
    # 1. 声明了就必须有解析分支
    if ! grep -qx -- "$name" <<<"$p"; then
      printf '  ✗ %s 声明了但无解析分支\n' "$name"; fail=1
      continue
    fi
    # 2. 端到端：真的跑一次 --help，确认它出现在输出里
    #    注意：pack to variable 再 grep，避免 pipefail 下 grep -q 提前关闭
    #    管道导致产生方 SIGPIPE(141) 的假阴性。
    h="$(bash "$f" --help 2>&1)"
    if ! grep -qF -- "$name" <<<"$h"; then
      printf '  ✗ %s 声明了但 --help 输出中查不到\n' "$name"; fail=1
    fi
  done <<< "$d"

  # 3. 注释里提到但没声明 = 误导
  while read -r name; do
    [[ -z "$name" ]] && continue
    if ! grep -qx -- "$name" <<<"$d"; then
      printf '  ✗ %s 出现在本脚本注释里但未用 flag() 声明\n' "$name"; fail=1
    fi
  done <<< "$c"

  # 4. 已废弃 flag 不得复活
  for old in "${REMOVED[@]}"; do
    if grep -qx -- "$old" <<<"$p"; then
      printf '  ✗ 已废弃的 %s 又出现在解析分支里\n' "$old"; fail=1
    fi
  done
done

echo
echo "== 环境变量规范名核对 =="
# 废弃别名（ANDROID_NDK_HOME/NDK_HOME/ANDROID_HOME/TARGET_ARCH）只允许
# 出现在 _common.sh 的兼容层；若被 build-*.sh 直接引用就是多通道输入复活。
for legacy in ANDROID_NDK_HOME NDK_HOME ANDROID_HOME TARGET_ARCH; do
  hits=$(grep -lE "\$\{?${legacy}[\}:]" scripts/build-*.sh 2>/dev/null || true)
  if [[ -z "$hits" ]]; then
    printf '  ✓ %s 仅在 _common.sh 兼容层出现\n' "$legacy"
  else
    printf '  ✗ %s 被 build 脚本直接引用: %s\n' "$legacy" "$hits"; fail=1
  fi
done

echo
[[ $fail -eq 0 ]] && echo "CLI 静态核对通过" || echo "CLI 静态核对失败"
exit $fail