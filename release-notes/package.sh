#!/usr/bin/env bash
# Assemble the evaluation tarball from a finished Release build.
set -euo pipefail

SRC=/home/chris/xbmc
BUILD=$SRC/build
ADDON_BUILD=$1                      # game.libretro build dir
ADDON_SRC=$3                        # game.libretro source dir
VER=$2
OUT=$SRC/dist/kodi-retroachievements-$VER

rm -rf "$OUT"; mkdir -p "$OUT"

cp "$BUILD/kodi-x11" "$OUT/"
strip "$OUT/kodi-x11"

cp -r "$BUILD/addons" "$BUILD/system" "$BUILD/media" "$OUT/"

# the paired add-on, without which achievements do nothing
mkdir -p "$OUT/addons/game.libretro"
cp -r "$ADDON_BUILD/game.libretro/." "$OUT/addons/game.libretro/"
cp -P "$ADDON_BUILD"/game.libretro.so* "$OUT/addons/game.libretro/"
cp -r "$ADDON_SRC/game.libretro/resources" "$OUT/addons/game.libretro/"
cp "$ADDON_SRC/game.libretro/icon.png" "$OUT/addons/game.libretro/"

# A core and its controller profile, so the build can be tried without first
# fetching anything from the add-on repository
for a in game.libretro.nestopia game.controller.nes; do
  for d in "$HOME/.kodi/addons/$a" "$HOME/kodi-test-data/addons/$a"; do
    [ -d "$d" ] && { cp -r "$d" "$OUT/addons/"; break; }
  done
done

cp "$SRC/release-notes/run-kodi.sh" "$OUT/"
cp "$SRC/release-notes/RETROACHIEVEMENTS.md" "$OUT/README.md"
chmod +x "$OUT/run-kodi.sh"

cd "$SRC/dist"
tar -cJf "kodi-retroachievements-$VER-linux-x86_64.tar.xz" "kodi-retroachievements-$VER"
ls -lh "kodi-retroachievements-$VER-linux-x86_64.tar.xz"
