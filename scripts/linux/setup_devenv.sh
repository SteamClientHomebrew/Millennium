#!/usr/bin/env bash

# Sets up the local dev environment by symlinking build outputs into
# the Steam runtime directories so you can test without installing.
#
# Only needs to be run once — the symlinks point into the build tree,
# so rebuilding automatically picks up changes without re-running this.
#
# Usage:
#   ./scripts/linux/setup_devenv.sh [BUILD_DIR]
#
# BUILD_DIR defaults to ./build (relative to repo root).
# Run from the repository root, or pass an absolute build path.
# To undo, run: ./scripts/linux/destroy_devenv.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-${REPO_ROOT}/build}"

BUILD_DIR="$(cd "${BUILD_DIR}" 2>/dev/null && pwd)" || {
    echo "error: build directory '${1:-build}' does not exist. Run cmake/ninja first."
    exit 1
}

BOOTSTRAP_X86="${BUILD_DIR}/libmillennium_bootstrap_x86.so"
BOOTSTRAP_HHX64="${BUILD_DIR}/libmillennium_bootstrap_hhx64.so"
MAIN_X86="${BUILD_DIR}/libmillennium_x86.so"
LUAVM="${BUILD_DIR}/libmillennium_luavm_x86"

# Verify all binaries exist
missing=0
for bin in "${BOOTSTRAP_X86}" "${BOOTSTRAP_HHX64}" "${MAIN_X86}" "${LUAVM}"; do
    if [ ! -f "${bin}" ]; then
        echo "error: missing ${bin}"
        missing=1
    fi
done
[ "${missing}" -eq 1 ] && exit 1

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

if [ ! -d "${U32}" ] || [ ! -d "${U64}" ]; then
    echo "error: Steam runtime dirs not found (${U32}, ${U64})."
    echo "       Launch Steam at least once first."
    exit 1
fi

echo "repo:  ${REPO_ROOT}"
echo "build: ${BUILD_DIR}"
echo "steam: ${STEAM_DIR}"
echo ""

ln -sfv "${BOOTSTRAP_X86}" "${U32}/libXtst.so.6"
ln -sfv "${BOOTSTRAP_HHX64}" "${U64}/libXtst.so.6"

rm -f "${STEAM_DIR}/.cef-enable-remote-debugging"

echo ""
echo "dev environment ready. restart Steam to pick up changes."
echo "this only needs to be run once; rebuilds are picked up automatically."
echo "to undo: ./scripts/linux/destroy_devenv.sh"
