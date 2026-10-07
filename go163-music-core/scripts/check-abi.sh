#!/usr/bin/env bash
#
# check-abi.sh - 静态核对 C/Go/C++ 三侧的状态码与导出符号一致性。
#
# 这类不同步的 bug 不会在编译期暴露，只在 UI 收到一个语义错误的
# 状态码时才暴露（例如把"上游异常"显示成"内部错误"）。所以每次改动
# 枚举或导出函数后都应跑一遍。
#
# 纯文本比对，不编译。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
fail=0
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "== 状态码三方核对 =="

# 用"取值所在字段的下标"而不是正则捕获来解析，避免行尾注释混进值里。
# 名字统一去掉下划线并转小写，使 NCM_ERR_INVALID_ARG 与 InvalidArg 可比。

# ncm.h:  NCM_ERR_X_Y = -1, /* 注释 */
awk '/NCM_(OK|ERR_[A-Z_]+) *= *-?[0-9]+/ {
  sym = $1; sub(/^NCM_/, "", sym); sub(/^ERR_/, "", sym)
  val = $3; sub(/,/, "", val)
  key = sym; gsub(/_/, "", key); key = tolower(key)
  print key, val
}' include/ncm.h | sort > "$tmp/h.txt"

# ncm.hpp:  Name = NCM_ERR_X_Y,
awk '/^[[:space:]]+[A-Za-z]+[[:space:]]*=[[:space:]]*NCM_(OK|ERR_[A-Z_]+)/ {
  name = $1
  sym  = $3; sub(/,$/, "", sym)
  key = name; gsub(/_/, "", key); key = tolower(key)
  print key, sym
}' include/ncm.hpp | sort > "$tmp/hpp_sym.txt"

# errs/status.go:  Name   Status = -1  // 注释
awk '/^[[:space:]]+[A-Za-z]+[[:space:]]+Status[[:space:]]*=/ {
  name = $1
  val = ""
  for (i = 1; i <= NF; i++) if ($i == "Status") { val = $(i + 2); break }
  gsub(/[,;]/, "", val)
  key = name; gsub(/_/, "", key); key = tolower(key)
  print key, val
}' internal/errs/status.go | sort > "$tmp/go.txt"

# 把 hpp 的符号回查成数值
awk 'NR == FNR { v[$1] = $2; next }
     { s = $2; sub(/^NCM_/, "", s); sub(/^ERR_/, "", s)
       gsub(/_/, "", s); s = tolower(s)
       print $1, (s in v ? v[s] : "MISSING") }' \
  "$tmp/h.txt" "$tmp/hpp_sym.txt" > "$tmp/hpp.txt"

for f in h go hpp; do
  printf '  %-4s %2s 项\n' "$f" "$(wc -l < "$tmp/$f.txt")"
done

if diff -u "$tmp/h.txt" "$tmp/go.txt"; then
  echo "  ✓ ncm.h ↔ errs.Status"
else
  echo "  ✗ ncm.h ↔ errs.Status 不一致"; fail=1
fi

if diff -u "$tmp/h.txt" "$tmp/hpp.txt"; then
  echo "  ✓ ncm.h ↔ ncm.hpp"
else
  echo "  ✗ ncm.h ↔ ncm.hpp 不一致"; fail=1
fi

echo
echo "== 导出符号三方核对 =="
grep -oE 'NCM_CALL[[:space:]]+ncm_[a-z_]+' include/ncm.h | grep -oE 'ncm_[a-z_]+' | sort -u > "$tmp/sym_h.txt"
grep -oE '^func (ncm_[a-z_]+)\(' ncmffi/ffi.go | sed -E 's/^func //; s/\($//' | sort -u > "$tmp/sym_go.txt"
grep -oE '\bncm_[a-z_]+\(' include/ncm.hpp | tr -d '(' | sort -u > "$tmp/sym_hpp.txt"

printf '  ncm.h %s / ffi.go %s / ncm.hpp %s\n' \
  "$(wc -l < "$tmp/sym_h.txt")" "$(wc -l < "$tmp/sym_go.txt")" "$(wc -l < "$tmp/sym_hpp.txt")"

if diff -u "$tmp/sym_h.txt" "$tmp/sym_go.txt"; then
  echo "  ✓ ncm.h ↔ ffi.go //export"
else
  echo "  ✗ ncm.h ↔ ffi.go 不一致"; fail=1
fi

missing=""
while read -r s; do grep -qx "$s" "$tmp/sym_hpp.txt" || missing="$missing $s"; done < "$tmp/sym_h.txt"
if [[ -z "$missing" ]]; then
  echo "  ✓ ncm.hpp 覆盖全部导出"
else
  echo "  ✗ ncm.hpp 未覆盖:$missing"; fail=1
fi

echo
[[ $fail -eq 0 ]] && echo "ABI 静态核对通过" || echo "ABI 静态核对失败"
exit $fail