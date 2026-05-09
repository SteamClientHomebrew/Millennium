#!/usr/bin/env bash

# Removes the symlinks created by setup_devenv.sh, restoring Steam
# to its stock state (Millennium will no longer load on next launch).

set -euo pipefail

if [ -d "${HOME}/.steam/steam" ]; then
    STEAM_DIR="${HOME}/.steam/steam"
elif [ -d "${HOME}/.local/share/Steam" ]; then
    STEAM_DIR="${HOME}/.local/share/Steam"
else
    echo "error: could not find Steam installation."
    exit 1
fi

U32="${STEAM_DIR}/ubuntu12_32"
U64="${STEAM_DIR}/ubuntu12_64"

remove_if_symlink() {
    if [ -L "$1" ]; then
        rm -v "$1"
    fi
}

remove_if_symlink "${U32}/libXtst.so.6"
remove_if_symlink "${U64}/libXtst.so.6"

echo ""
echo "dev environment removed. restart Steam to apply."
