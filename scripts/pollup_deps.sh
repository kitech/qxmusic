
set -euo pipefail
set -x

pwd

# Pull dependency repos referenced by repository-root symlinks:
#   qlcomp  -> ../qxcomp/qlcomp
#   qskcomp -> ../qxcomp/qskcomp
#   qlappcom -> ../anystik/qlappcom
#   vendor  -> ../cxvendor/vendor
#
# A fresh CI checkout has dangling symlinks, so we clone the remote
# repositories into the workspace parent directory (same mechanism as
# doxhttpd/.github/workflows/ci.yml).

# --depth 1：CI 只需要快照，省时省流（doxhttpd 同款做法）
git clone --depth 1 https://github.com/qxcomp/qxcomp.git ../qxcomp
git clone --depth 1 https://github.com/qxcomp/cxvendor.git ../cxvendor
git clone --depth 1 https://github.com/kitech/anystik.git ../anystik

# 软链目标缺失会在 qmake/cmake 里静默跳过，表现为 "not found" 或链接错误，
# 这里直接失败并给出明确原因
test -d ../qxcomp/qlcomp    || { echo "FATAL: ../qxcomp/qlcomp missing"; exit 1; }
test -d ../qxcomp/qskcomp   || { echo "FATAL: ../qxcomp/qskcomp missing"; exit 1; }
test -d ../anystik/qlappcom || { echo "FATAL: ../anystik/qlappcom missing"; exit 1; }
test -d ../cxvendor/vendor  || { echo "FATAL: ../cxvendor/vendor missing"; exit 1; }
