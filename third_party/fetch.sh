#!/bin/sh
# Re-download the vendored audio third-party sources into this directory.
# Run from the repo root:  sh third_party/fetch.sh
# If GitHub is slow/blocked, prefix URLs with https://gh-proxy.com/
set -e
cd "$(dirname "$0")"
PROXY="${GHPROXY:-}"

dl() { # dl <url> <out>
  if [ -n "$PROXY" ]; then
    curl -fsSL -o "$2" "${PROXY}$1"
  else
    curl -fsSL -o "$2" "$1" || curl -fsSL -o "$2" "https://gh-proxy.com/$1"
  fi
}

mkdir -p miniaudio stb minimp4 mpv/include/mpv faad2

dl https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h \
   miniaudio/miniaudio.h
dl https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/extras/stb_vorbis.c \
   stb/stb_vorbis.c
dl https://raw.githubusercontent.com/lieff/minimp4/2a3014a/minimp4.h \
   minimp4/minimp4.h
dl https://raw.githubusercontent.com/mpv-player/mpv/v0.41.0/include/mpv/client.h \
   mpv/include/mpv/client.h

# FAAD2 2.11.3 (needs include/ + libfaad/ only)
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
dl https://github.com/FreewareAdvancedAudio/faad2/archive/refs/tags/2.11.3.tar.gz \
   "$tmp/faad2.tar.gz"
tar xzf "$tmp/faad2.tar.gz" -C "$tmp"
src="$(ls -d "$tmp"/faad2-2.11.3)"
rm -rf faad2/include faad2/libfaad
cp -r "$src/include" faad2/include
cp -r "$src/libfaad" faad2/libfaad
cp "$src/COPYING" faad2/COPYING
cp "$src/CMakeLists.txt" faad2/CMakeLists.txt

echo "third_party: done"
