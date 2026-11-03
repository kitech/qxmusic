#!/bin/sh
# Pull dependency repos referenced by repository-root symlinks:
#   qlcomp  -> ../qxcomp/qlcomp
#   qskcomp -> ../qxcomp/qskcomp
#   qlappcom -> ../anystik/qlappcom
#
# A fresh CI checkout has dangling symlinks, so we clone the remote
# repositories into the workspace parent directory (same mechanism as
# doxhttpd/.github/workflows/ci.yml).
set -e
set -x
pwd
git clone https://github.com/qxcomp/qxcomp.git ../qxcomp
git clone https://github.com/kitech/anystik.git ../anystik
