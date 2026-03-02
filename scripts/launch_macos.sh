#!/usr/bin/env bash

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly DEFAULT_WRAPPER_BUILD_DIR="${REPO_ROOT}/build/osx-debug-make/src/boot/macos"
readonly DEFAULT_RUNTIME_SEARCH_DIRS=(
    "${REPO_ROOT}/build/osx-debug-make/src"
    "${REPO_ROOT}/build/osx-release-make/src"
    "${REPO_ROOT}/build/osx-debug/src"
    "${REPO_ROOT}/build/osx-release/src"
    "${REPO_ROOT}/build"
)
readonly DEFAULT_STEAM_EXECUTABLE="${HOME}/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steam_osx"

WRAPPER_BUILD_DIR="${DEFAULT_WRAPPER_BUILD_DIR}"
RUNTIME_BUILD_DIR=""
STEAM_EXECUTABLE="${DEFAULT_STEAM_EXECUTABLE}"
ENABLE_DEV_MODE=0
EXPLICIT_DEVTOOLS_PORT=""

usage() {
    cat <<EOF
Usage: $(basename "$0") [--wrapper-build-dir <dir>] [--runtime-build-dir <dir>] [--steam-executable <path>] [--] [steam args...]

Launches Steam on macOS without modifying Steam's runtime bundle. The launcher:
1. Loads libmillennium_bootstrap.dylib into steam_osx
2. Creates Steam's .cef-enable-remote-debugging marker file beside the runtime
3. Adds -devtools-port automatically unless you already passed one
4. Points bootstrap at libmillennium.dylib
5. Lets bootstrap hand off future child injection to libmillennium_child_hook.dylib
6. Injects libmillennium_hhx64.dylib alongside the child hook so Steam Helper gets CEF hooks before startup

Developer tools helpers:
- Use --dev to append Steam's -dev flag (required on macOS for F10 / Ctrl+Shift+I).
- Use --devtools-port <port> to force a specific devtools port.
- You can still pass raw Steam args after --.

Once Millennium is installed, open it from the Steam menu in the macOS menu bar
(next to the Apple logo) by selecting Millennium.
You can also open Millennium from the steam url:
  open 'steam://millennium/settings'

Example:
  ./scripts/launch_macos.sh

Debug example:
  MILLENNIUM_BOOTSTRAP_TRACE_PATH=/tmp/millennium-bootstrap.trace \\
  MILLENNIUM_HHX_TRACE_PATH=/tmp/millennium-hhx.trace \\
  ./scripts/launch_macos.sh

DevTools example:
  ./scripts/launch_macos.sh --dev --devtools-port 8080
EOF
}

ensure_steam_not_running() {
    if pgrep -x "steam_osx" >/dev/null 2>&1 || pgrep -f "/Steam Helper" >/dev/null 2>&1; then
        printf "Steam appears to already be running. Quit Steam before launching through %s.\n" "$(basename "$0")" >&2
        exit 1
    fi
}

find_artifact() {
    local artifact_name="$1"
    shift
    local result

    for search_dir in "$@"; do
        [ -n "${search_dir}" ] || continue
        result=$(find "${search_dir}" -type f -name "${artifact_name}" -print -quit 2>/dev/null || true)
        if [ -n "${result}" ]; then
            printf "%s\n" "${result}"
            return 0
        fi
    done

    printf "Missing build artifact '%s'.\n" "${artifact_name}" >&2
    exit 1
}

ensure_bootstrap_assets() {
    local missing_assets=0
    local loader_entry="${REPO_ROOT}/src/sdk/packages/loader/build/millennium.js"
    local chunks_dir="${REPO_ROOT}/src/sdk/packages/loader/build/chunks"
    local required_assets=(
        "${REPO_ROOT}/build/frontend.bin"
        "${loader_entry}"
    )
    local required_chunks=()
    local chunk_name

    for asset_path in "${required_assets[@]}"; do
        if [ ! -r "${asset_path}" ]; then
            printf "Missing macOS bootstrap asset: %s\n" "${asset_path}" >&2
            missing_assets=1
        fi
    done

    if [ "${missing_assets}" -eq 0 ]; then
        mapfile -t required_chunks < <(
            grep -oE "chunks/chunk-[A-Za-z0-9._-]+\\.js" "${loader_entry}" \
                | sed 's#^chunks/##' \
                | sort -u \
                || true
        )

        if [ "${#required_chunks[@]}" -eq 0 ]; then
            for chunk_path in "${chunks_dir}"/chunk-*.js; do
                [ -e "${chunk_path}" ] || continue
                required_chunks+=("$(basename "${chunk_path}")")
            done
        fi

        if [ "${#required_chunks[@]}" -eq 0 ]; then
            printf "Missing macOS bootstrap asset: no chunk-*.js files in %s\n" "${chunks_dir}" >&2
            missing_assets=1
        fi
    fi

    for chunk_name in "${required_chunks[@]}"; do
        if [ ! -r "${chunks_dir}/${chunk_name}" ]; then
            printf "Missing macOS bootstrap asset: %s/%s\n" "${chunks_dir}" "${chunk_name}" >&2
            missing_assets=1
        fi
    done

    if [ "${missing_assets}" -eq 0 ]; then
        return 0
    fi

    cat >&2 <<EOF
Build the missing bootstrap assets before launching Steam:
  rm -rf src/sdk/packages/client/build
  pnpm -C src/sdk/packages/loader build
  pnpm -C src/sdk/packages/client build
  pnpm -C src/frontend build
EOF
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --wrapper-build-dir)
            [ $# -ge 2 ] || {
                printf "Missing value for --wrapper-build-dir\n" >&2
                exit 1
            }
            WRAPPER_BUILD_DIR="$2"
            shift 2
            ;;
        --runtime-build-dir)
            [ $# -ge 2 ] || {
                printf "Missing value for --runtime-build-dir\n" >&2
                exit 1
            }
            RUNTIME_BUILD_DIR="$2"
            shift 2
            ;;
        --steam-executable)
            [ $# -ge 2 ] || {
                printf "Missing value for --steam-executable\n" >&2
                exit 1
            }
            STEAM_EXECUTABLE="$2"
            shift 2
            ;;
        --dev)
            ENABLE_DEV_MODE=1
            shift
            ;;
        --devtools-port)
            [ $# -ge 2 ] || {
                printf "Missing value for --devtools-port\n" >&2
                exit 1
            }
            EXPLICIT_DEVTOOLS_PORT="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
    esac
done

[ -x "${STEAM_EXECUTABLE}" ] || {
    printf "Steam executable not found: %s\n" "${STEAM_EXECUTABLE}" >&2
    exit 1
}

ensure_bootstrap_assets
ensure_steam_not_running

declare -a runtime_search_dirs
if [ -n "${RUNTIME_BUILD_DIR}" ]; then
    runtime_search_dirs=("${RUNTIME_BUILD_DIR}")
else
    runtime_search_dirs=("${DEFAULT_RUNTIME_SEARCH_DIRS[@]}")
fi

launcher_path=$(find_artifact "steam_osx" "${WRAPPER_BUILD_DIR}")
runtime_path=$(find_artifact "libmillennium.dylib" "${runtime_search_dirs[@]}")
hook_helper_path=$(find_artifact "libmillennium_hhx64.dylib" "${runtime_search_dirs[@]}")
child_hook_path=$(find_artifact "libmillennium_child_hook.dylib" "${WRAPPER_BUILD_DIR}")
launcher_path=$(realpath "${launcher_path}")
runtime_path=$(realpath "${runtime_path}")
hook_helper_path=$(realpath "${hook_helper_path}")
child_hook_path=$(realpath "${child_hook_path}")
STEAM_EXECUTABLE=$(realpath "${STEAM_EXECUTABLE}")

export MILLENNIUM_STEAM_EXECUTABLE="${STEAM_EXECUTABLE}"
export MILLENNIUM_RUNTIME_PATH="${runtime_path}"
export MILLENNIUM_HOOK_HELPER_PATH="${hook_helper_path}"
export MILLENNIUM_CHILD_HOOK_PATH="${child_hook_path}"

declare -a steam_args
steam_args=("$@")

if [ "${ENABLE_DEV_MODE}" -eq 1 ]; then
    has_dev_flag=0
    for arg in "${steam_args[@]}"; do
        if [ "${arg}" = "-dev" ]; then
            has_dev_flag=1
            break
        fi
    done

    if [ "${has_dev_flag}" -eq 0 ]; then
        steam_args+=("-dev")
    fi
fi

if [ -n "${EXPLICIT_DEVTOOLS_PORT}" ]; then
    has_devtools_port=0
    for arg in "${steam_args[@]}"; do
        case "${arg}" in
            -devtools-port|-devtools-port=*)
                has_devtools_port=1
                break
                ;;
        esac
    done

    if [ "${has_devtools_port}" -eq 0 ]; then
        steam_args+=("-devtools-port" "${EXPLICIT_DEVTOOLS_PORT}")
    fi
fi

exec "${launcher_path}" "${steam_args[@]}"
