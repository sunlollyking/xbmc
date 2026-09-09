#!/usr/bin/env bash
#
# Run this build without touching an existing Kodi installation. Everything it
# reads and writes stays inside this folder.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export KODI_HOME="$HERE"
export KODI_DATA="$HERE/userdata-profile"

mkdir -p "$KODI_DATA"

exec "$HERE/kodi-x11" "$@"
