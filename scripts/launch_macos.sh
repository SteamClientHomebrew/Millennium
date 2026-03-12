#!/usr/bin/env bash

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly DEFAULT_STEAM_EXECUTABLE="${HOME}/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steam_osx"
readonly DEFAULT_WRAPPER_SEARCH_DIRS=(
    "${REPO_ROOT}/build/osx-debug/src/bootstrap/macos"
    "${REPO_ROOT}/build/osx-release/src/bootstrap/macos"
    "${REPO_ROOT}/build/src/bootstrap/macos"
    "${REPO_ROOT}/build"
)
readonly DEFAULT_RUNTIME_SEARCH_DIRS=(
    "${REPO_ROOT}/build/osx-debug/src"
    "${REPO_ROOT}/build/osx-release/src"
    "${REPO_ROOT}/build/src"
    "${REPO_ROOT}/build"
)
readonly RUNTIME_ARTIFACT_CANDIDATES=(
    "libmillennium.dylib"
)
readonly HOOK_HELPER_ARTIFACT_CANDIDATES=(
    "libmillennium_hhx64.dylib"
)
readonly CHILD_HOOK_ARTIFACT_CANDIDATES=(
    "libmillennium_child_hook.dylib"
)

WRAPPER_BUILD_DIR=""
RUNTIME_BUILD_DIR=""
STEAM_EXECUTABLE="${DEFAULT_STEAM_EXECUTABLE}"
ENABLE_DEV_MODE=0
EXPLICIT_DEVTOOLS_PORT=""
EXPLICIT_NATIVE_CORNERS=""
EXPLICIT_NATIVE_CORNER_RADIUS=""
PASSTHROUGH_STEAM_ARGS=()

usage() {
    cat <<EOF
Usage: $(basename "$0") [--build-dir <dir>] [--wrapper-build-dir <dir>] [--runtime-build-dir <dir>] [--steam-executable <path>] [--dev] [--devtools-port <port>] [--native-corners <auto|off|force>] [--native-corner-radius <number>] [--] [steam args...]

Launches Steam on macOS through Millennium's wrapper flow:
1. Resolves steam_osx wrapper and runtime artifacts from your build output
2. Exports runtime/hook paths for the wrapper/bootstrap process
3. Launches Steam with optional DevTools flags

Options:
  --build-dir <dir>         Set both wrapper/runtime artifact search to one directory
  --wrapper-build-dir <dir> Override wrapper artifact search root
  --runtime-build-dir <dir> Override runtime artifact search root
  --steam-executable <path> Override Steam runtime executable path
  --dev                     Append -dev when missing
  --devtools-port <port>    Force -devtools-port when missing
  --native-corners <auto|off|force>
                            auto: style frameless/custom-chrome windows only (default)
                            off: disable native corner styling
                            force: style all normal app windows
  --native-corner-radius <number>
                            Override corner radius (clamped by runtime to 6..20)

Examples:
  ./scripts/launch_macos.sh
  ./scripts/launch_macos.sh --build-dir ./build/osx-debug
  ./scripts/launch_macos.sh --dev --devtools-port 8080
  ./scripts/launch_macos.sh --native-corner-radius 12
EOF
}

fail() {
    printf "%s\n" "$1" >&2
    exit 1
}

abspath_existing() {
    local target="$1"

    if [ -d "${target}" ]; then
        (
            cd "${target}" >/dev/null 2>&1 || exit 1
            pwd -P
        )
        return $?
    fi

    local parent
    parent="$(dirname "${target}")"
    local base
    base="$(basename "${target}")"

    (
        cd "${parent}" >/dev/null 2>&1 || exit 1
        printf "%s/%s\n" "$(pwd -P)" "${base}"
    )
}

ensure_steam_not_running() {
    local matched_processes
    matched_processes="$(ps -axo pid,ppid,comm,args 2>/dev/null | grep -E "[s]team_osx|[S]team Helper" || true)"

    if [ -n "${matched_processes}" ]; then
        fail "Steam appears to already be running. Quit Steam before launching through $(basename "$0").
Matched processes:
${matched_processes}"
    fi
}

find_artifact_by_names() {
    local names=()
    local dirs=()
    local name=""
    local dir=""
    local result=""

    while [ $# -gt 0 ]; do
        if [ "$1" = "--" ]; then
            shift
            break
        fi
        names+=("$1")
        shift
    done

    while [ $# -gt 0 ]; do
        dirs+=("$1")
        shift
    done

    for dir in "${dirs[@]}"; do
        [ -n "${dir}" ] || continue
        [ -d "${dir}" ] || continue

        for name in "${names[@]}"; do
            result="$(find "${dir}" -type f -name "${name}" -print -quit 2>/dev/null || true)"
            if [ -n "${result}" ]; then
                printf "%s\n" "${result}"
                return 0
            fi
        done
    done

    return 1
}

resolve_bootstrap_asset_paths() {
    local frontend_candidates=(
        "${REPO_ROOT}/src/typescript/.frontend.bin"
    )
    local loader_candidates=(
        "${REPO_ROOT}/src/typescript/sdk/packages/loader/build/millennium.js"
    )

    local frontend_asset=""
    local loader_entry=""
    local candidate=""

    for candidate in "${frontend_candidates[@]}"; do
        if [ -r "${candidate}" ]; then
            frontend_asset="${candidate}"
            break
        fi
    done

    for candidate in "${loader_candidates[@]}"; do
        if [ -r "${candidate}" ]; then
            loader_entry="${candidate}"
            break
        fi
    done

    if [ -z "${frontend_asset}" ]; then
        fail "Missing macOS bootstrap asset: expected frontend bundle in one of: ${frontend_candidates[*]}"
    fi

    if [ -z "${loader_entry}" ]; then
        fail "Missing macOS bootstrap asset: expected loader entry in one of: ${loader_candidates[*]}"
    fi

    printf "%s\n%s\n" "${frontend_asset}" "${loader_entry}"
}

ensure_bootstrap_assets() {
    local resolved_assets=""
    resolved_assets="$(resolve_bootstrap_asset_paths)"

    local frontend_asset
    frontend_asset="$(printf "%s\n" "${resolved_assets}" | sed -n '1p')"
    local loader_entry
    loader_entry="$(printf "%s\n" "${resolved_assets}" | sed -n '2p')"
    local chunks_dir
    chunks_dir="$(dirname "${loader_entry}")/chunks"

    local missing_assets=0
    local required_chunks=()
    local chunk_name=""
    local has_chunk_marker=0

    if [ ! -d "${chunks_dir}" ]; then
        printf "Missing macOS bootstrap asset: %s\n" "${chunks_dir}" >&2
        missing_assets=1
    fi

    while IFS= read -r chunk_name; do
        [ -n "${chunk_name}" ] || continue
        required_chunks+=("${chunk_name}")
    done < <(
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

    for chunk_name in "${required_chunks[@]}"; do
        has_chunk_marker=1
        if [ ! -r "${chunks_dir}/${chunk_name}" ]; then
            printf "Missing macOS bootstrap asset: %s/%s\n" "${chunks_dir}" "${chunk_name}" >&2
            missing_assets=1
        fi
    done

    if [ "${has_chunk_marker}" -eq 0 ]; then
        printf "Missing macOS bootstrap asset: no chunk-*.js files in %s\n" "${chunks_dir}" >&2
        missing_assets=1
    fi

    if [ "${missing_assets}" -eq 0 ]; then
        return 0
    fi

    cat >&2 <<EOF
Build the missing bootstrap assets before launching Steam.

Current branch layout:
  (cd src/typescript/ttc && bun run build)
  (cd src/typescript/sdk/packages/loader && bun run build)
  (cd src/typescript/frontend && bun run build)
  # or run all together:
  (cd src/typescript && bun run build)

Expected frontend artifact:
  ${frontend_asset}
EOF
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ $# -ge 2 ] || fail "Missing value for --build-dir"
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
        --steam-executable)
            [ $# -ge 2 ] || fail "Missing value for --steam-executable"
            STEAM_EXECUTABLE="$2"
            shift 2
            ;;
        --dev)
            ENABLE_DEV_MODE=1
            shift
            ;;
        --devtools-port)
            [ $# -ge 2 ] || fail "Missing value for --devtools-port"
            EXPLICIT_DEVTOOLS_PORT="$2"
            shift 2
            ;;
        --native-corners)
            [ $# -ge 2 ] || fail "Missing value for --native-corners"
            EXPLICIT_NATIVE_CORNERS="$2"
            shift 2
            ;;
        --native-corner-radius)
            [ $# -ge 2 ] || fail "Missing value for --native-corner-radius"
            EXPLICIT_NATIVE_CORNER_RADIUS="$2"
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
            PASSTHROUGH_STEAM_ARGS+=("$1")
            shift
            ;;
    esac
done

[ -x "${STEAM_EXECUTABLE}" ] || fail "Steam executable not found: ${STEAM_EXECUTABLE}"

ensure_bootstrap_assets
ensure_steam_not_running

declare -a wrapper_search_dirs
declare -a runtime_search_dirs

if [ -n "${WRAPPER_BUILD_DIR}" ]; then
    wrapper_search_dirs=("${WRAPPER_BUILD_DIR}")
else
    wrapper_search_dirs=("${DEFAULT_WRAPPER_SEARCH_DIRS[@]}")
fi

if [ -n "${RUNTIME_BUILD_DIR}" ]; then
    runtime_search_dirs=("${RUNTIME_BUILD_DIR}")
else
    runtime_search_dirs=("${DEFAULT_RUNTIME_SEARCH_DIRS[@]}")
fi

launcher_path="$(find_artifact_by_names "steam_osx" -- "${wrapper_search_dirs[@]}" || true)"
[ -n "${launcher_path}" ] || fail "Missing build artifact: steam_osx. Checked: ${wrapper_search_dirs[*]}
Build with:
  cmake --preset osx-debug
  cmake --build --preset osx-debug -j"

runtime_path="$(find_artifact_by_names "${RUNTIME_ARTIFACT_CANDIDATES[@]}" -- "${runtime_search_dirs[@]}" || true)"
[ -n "${runtime_path}" ] || fail "Missing runtime artifact. Tried: ${RUNTIME_ARTIFACT_CANDIDATES[*]} in ${runtime_search_dirs[*]}
Build with:
  cmake --preset osx-debug
  cmake --build --preset osx-debug -j"

hook_helper_path="$(find_artifact_by_names "${HOOK_HELPER_ARTIFACT_CANDIDATES[@]}" -- "${runtime_search_dirs[@]}" || true)"
[ -n "${hook_helper_path}" ] || fail "Missing helper hook artifact. Tried: ${HOOK_HELPER_ARTIFACT_CANDIDATES[*]} in ${runtime_search_dirs[*]}
Build with:
  cmake --preset osx-debug
  cmake --build --preset osx-debug -j"

child_hook_path="$(find_artifact_by_names "${CHILD_HOOK_ARTIFACT_CANDIDATES[@]}" -- "${wrapper_search_dirs[@]}" "${runtime_search_dirs[@]}" || true)"
[ -n "${child_hook_path}" ] || fail "Missing child hook artifact. Tried: ${CHILD_HOOK_ARTIFACT_CANDIDATES[*]} in ${wrapper_search_dirs[*]} ${runtime_search_dirs[*]}
Build with:
  cmake --preset osx-debug
  cmake --build --preset osx-debug -j"

launcher_path="$(abspath_existing "${launcher_path}")"
runtime_path="$(abspath_existing "${runtime_path}")"
hook_helper_path="$(abspath_existing "${hook_helper_path}")"
child_hook_path="$(abspath_existing "${child_hook_path}")"
STEAM_EXECUTABLE="$(abspath_existing "${STEAM_EXECUTABLE}")"

export MILLENNIUM_STEAM_EXECUTABLE="${STEAM_EXECUTABLE}"
export MILLENNIUM_RUNTIME_PATH="${runtime_path}"
export MILLENNIUM_HOOK_HELPER_PATH="${hook_helper_path}"
export MILLENNIUM_CHILD_HOOK_PATH="${child_hook_path}"

if [ -n "${EXPLICIT_NATIVE_CORNERS}" ]; then
    export MILLENNIUM_MACOS_NATIVE_CORNERS="${EXPLICIT_NATIVE_CORNERS}"
fi

if [ -n "${EXPLICIT_NATIVE_CORNER_RADIUS}" ]; then
    export MILLENNIUM_MACOS_NATIVE_CORNER_RADIUS="${EXPLICIT_NATIVE_CORNER_RADIUS}"
fi

declare -a steam_args
steam_args=("${PASSTHROUGH_STEAM_ARGS[@]}" "$@")

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

if [ "${#steam_args[@]}" -gt 0 ]; then
    exec "${launcher_path}" "${steam_args[@]}"
else
    exec "${launcher_path}"
fi
