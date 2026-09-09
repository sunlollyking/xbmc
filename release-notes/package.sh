#!/usr/bin/env bash
#
# Assemble the evaluation tarball from a finished Release build.
#
#   package.sh <version> <game.libretro build dir> <game.libretro source dir>
set -euo pipefail

VER=$1
ADDON_BUILD=$2
ADDON_SRC=$3

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=$SRC/build
OUT=$SRC/dist/kodi-retroachievements-$VER

rm -rf "$OUT"; mkdir -p "$OUT"

cp "$BUILD/kodi-x11" "$OUT/"
strip "$OUT/kodi-x11"

cp -r "$BUILD/addons" "$BUILD/system" "$BUILD/media" "$OUT/"

# A build tree used for testing collects symlinks to add-ons kept elsewhere.
# They are not part of the build and must not reach the tarball, where they
# would either dangle or collide with the add-ons placed below.
find "$OUT/addons" -maxdepth 1 -type l -delete

# The paired add-on, without which achievements do nothing. Its resources and
# icon come from the source tree, which the build directory does not carry.
mkdir -p "$OUT/addons/game.libretro"
cp -r "$ADDON_BUILD/game.libretro/." "$OUT/addons/game.libretro/"
cp -P "$ADDON_BUILD"/game.libretro.so* "$OUT/addons/game.libretro/"
cp -r "$ADDON_SRC/game.libretro/resources" "$OUT/addons/game.libretro/"
cp "$ADDON_SRC/game.libretro/icon.png" "$OUT/addons/game.libretro/"

# A core and its controller profile, so the build can be tried without first
# fetching anything from the add-on repository. FCEUmm rather than Nestopia:
# the Nestopia build here fails retro_serialize(), which leaves the emulator
# unable to write a save state at all.
for a in game.libretro.fceumm game.controller.nes; do
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
