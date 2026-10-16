#!/usr/bin/env bash
# Verify the qmake source manifest (qlmusic/audio.pri) stays in sync with the
# actual cxaudio sources. qmake uses an explicit list (Qt3 has no $$files()),
# while CMake globs src/*.c + src/*.cpp, so qmake is the side that can drift.
set -euo pipefail

CXROOT="$(cd "$(dirname "$0")/.." && pwd)"
PRI="$CXROOT/../qlmusic/audio.pri"
FAAD_DIR="$CXROOT/../third_party/faad2/libfaad"

if [ ! -f "$PRI" ]; then
    echo "check_manifest: $PRI not found" >&2
    exit 2
fi

# 1) Sources declared in audio.pri (.c/.cpp only; ignores HEADERS/*.h).
declared="$(grep -oE '\$\$(AUDIO|THIRD)/[A-Za-z0-9_./-]+\.(c|cpp)' "$PRI" \
    | sed -E 's#.*/##' | sort -u)"

# 2) Sources actually present on disk (same set CMake globs).
actual="$( { ls -1 "$CXROOT/src"/*.cpp "$CXROOT/src"/*.c 2>/dev/null || true
            ls -1 "$FAAD_DIR"/*.c 2>/dev/null || true; } \
    | xargs -r -n1 basename | sort -u)"

missing_in_pri="$(comm -13 <(printf '%s\n' "$declared") <(printf '%s\n' "$actual") || true)"
stale_in_pri="$(comm -23 <(printf '%s\n' "$declared") <(printf '%s\n' "$actual") || true)"

if [ -n "$missing_in_pri" ] || [ -n "$stale_in_pri" ]; then
    echo "check_manifest: audio.pri source list OUT OF SYNC" >&2
    [ -n "$missing_in_pri" ] && printf '  on disk but missing in audio.pri:\n%s\n' "$missing_in_pri" >&2
    [ -n "$stale_in_pri" ]  && printf '  listed in audio.pri but absent on disk:\n%s\n' "$stale_in_pri" >&2
    exit 1
fi

echo "check_manifest: OK ($(printf '%s\n' "$actual" | wc -l) sources in sync)"
