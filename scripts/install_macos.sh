#!/usr/bin/env bash

# ==================================================
#   _____ _ _ _             _
#  |     |_| | |___ ___ ___|_|_ _ _____
#  | | | | | | | -_|   |   | | | |     |
#  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
#
# ==================================================

# Copyright (c) 2025 Project Millennium

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly DEFAULT_STEAM_DIR="${HOME}/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS"
readonly WRAPPER_REQUIRED_ARTIFACTS=(
    "steam_osx"
    "libmillennium_bootstrap.dylib"
    "libmillennium_child_hook.dylib"
)
readonly RUNTIME_REQUIRED_ARTIFACTS=(
    "libmillennium.dylib"
    "libmillennium_hhx64.dylib"
)

MODE="install"
LEGACY_BUILD_DIR=""
WRAPPER_BUILD_DIR="${REPO_ROOT}/build/macos-boot"
RUNTIME_BUILD_DIR=""
STEAM_DIR="${DEFAULT_STEAM_DIR}"
DRY_RUN=0

log() { printf "%b\n" "$1"; }

fail() {
    log "$1" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [--build-dir <dir>] [--wrapper-build-dir <dir>] [--runtime-build-dir <dir>] [--steam-dir <dir>] [--dry-run] [--uninstall]

Installs local macOS Millennium artifacts into Steam's runtime bundle:
1. Backing up steam_osx to steam_osx.real
2. Copying the wrapper to steam_osx
3. Copying the child hook + Millennium dylibs beside steam_osx

This is a debug fallback for legacy in-place replacement testing. For normal local
development, use ./scripts/launch_macos.sh.

Use --uninstall to restore steam_osx and remove injected dylibs.

--build-dir keeps the old behavior and searches one directory for all artifacts.
--wrapper-build-dir defaults to ${WRAPPER_BUILD_DIR}
--runtime-build-dir defaults to auto-detecting the top-level macOS build output.
EOF
}

run_cmd() {
    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] $*"
        return 0
    fi

    "$@"
}

ensure_steam_closed() {
    if pgrep -x "steam_osx" >/dev/null 2>&1 || pgrep -f "/Steam Helper" >/dev/null 2>&1; then
        fail "Quit Steam before modifying the runtime bundle."
    fi
}

find_artifact() {
    local name="$1"
    shift
    local result

    for search_dir in "$@"; do
        [ -n "${search_dir}" ] || continue
        result=$(find "${search_dir}" -type f -name "${name}" -print -quit 2>/dev/null || true)
        if [ -n "${result}" ]; then
            printf "%s\n" "${result}"
            return 0
        fi
    done

    fail "Missing build artifact '${name}'. Build the wrappers with ./scripts/build_macos_boot.sh and build the top-level project after ./scripts/bootstrap_deps.sh so libmillennium*.dylib is available."
}

install_file() {
    local source_path="$1"
    local destination_path="$2"

    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] install -m 0755 ${source_path} ${destination_path}"
        return 0
    fi

    install -m 0755 "${source_path}" "${destination_path}"
}

install_runtime() {
    local runtime_search_dirs=()

    ensure_steam_closed

    [ -d "${STEAM_DIR}" ] || fail "Steam runtime directory not found: ${STEAM_DIR}"
    [ -f "${STEAM_DIR}/steam_osx" ] || fail "Steam executable not found in ${STEAM_DIR}"

    if [ ! -f "${STEAM_DIR}/steam_osx.real" ]; then
        run_cmd mv "${STEAM_DIR}/steam_osx" "${STEAM_DIR}/steam_osx.real"
    else
        log "steam_osx.real already exists, preserving the existing backup."
    fi

    if [ -n "${LEGACY_BUILD_DIR}" ]; then
        runtime_search_dirs=("${LEGACY_BUILD_DIR}")
    elif [ -n "${RUNTIME_BUILD_DIR}" ]; then
        runtime_search_dirs=("${RUNTIME_BUILD_DIR}")
    else
        runtime_search_dirs=(
            "${REPO_ROOT}/build"
            "${REPO_ROOT}/build/osx-debug-make"
            "${REPO_ROOT}/build/osx-release-make"
            "${REPO_ROOT}/build/osx-debug"
            "${REPO_ROOT}/build/osx-release"
        )
    fi

    for artifact in "${WRAPPER_REQUIRED_ARTIFACTS[@]}"; do
        local source_path
        source_path=$(find_artifact "${artifact}" "${WRAPPER_BUILD_DIR}")
        install_file "${source_path}" "${STEAM_DIR}/${artifact}"
    done

    for artifact in "${RUNTIME_REQUIRED_ARTIFACTS[@]}"; do
        local source_path
        source_path=$(find_artifact "${artifact}" "${runtime_search_dirs[@]}")
        install_file "${source_path}" "${STEAM_DIR}/${artifact}"
    done
    log "Installed Millennium macOS runtime into ${STEAM_DIR}"
    log "Warning: this in-place mode is only for debugging and may trigger Steam file verification."
}

uninstall_runtime() {
    ensure_steam_closed

    [ -d "${STEAM_DIR}" ] || fail "Steam runtime directory not found: ${STEAM_DIR}"
    [ -f "${STEAM_DIR}/steam_osx.real" ] || fail "No steam_osx.real backup found in ${STEAM_DIR}"

    for artifact in "${WRAPPER_REQUIRED_ARTIFACTS[@]}"; do
        if [ -f "${STEAM_DIR}/${artifact}" ]; then
            run_cmd rm -f "${STEAM_DIR}/${artifact}"
        fi
    done

    for artifact in "${RUNTIME_REQUIRED_ARTIFACTS[@]}"; do
        if [ -f "${STEAM_DIR}/${artifact}" ]; then
            run_cmd rm -f "${STEAM_DIR}/${artifact}"
        fi
    done

    run_cmd mv "${STEAM_DIR}/steam_osx.real" "${STEAM_DIR}/steam_osx"

    log "Restored the original Steam runtime executables in ${STEAM_DIR}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ $# -ge 2 ] || fail "Missing value for --build-dir"
            LEGACY_BUILD_DIR="$2"
            WRAPPER_BUILD_DIR="$2"
            RUNTIME_BUILD_DIR="$2"
            shift 2
            ;;
        --wrapper-build-dir)
            [ $# -ge 2 ] || fail "Missing value for --wrapper-build-dir"
            WRAPPER_BUILD_DIR="$2"
            shift 2
            ;;
        --runtime-build-dir)
            [ $# -ge 2 ] || fail "Missing value for --runtime-build-dir"
            RUNTIME_BUILD_DIR="$2"
            shift 2
            ;;
        --steam-dir)
            [ $# -ge 2 ] || fail "Missing value for --steam-dir"
            STEAM_DIR="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --uninstall)
            MODE="uninstall"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "Unknown argument: $1"
            ;;
    esac
done

if [ "${MODE}" = "install" ]; then
    install_runtime
else
    uninstall_runtime
fi
