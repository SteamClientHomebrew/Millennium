#!/usr/bin/env bash

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIR="${REPO_ROOT}/src/boot/macos"
readonly DEFAULT_BUILD_DIR="${REPO_ROOT}/build/macos-boot"

BUILD_DIR="${DEFAULT_BUILD_DIR}"
BUILD_TYPE="Debug"
GENERATOR="Unix Makefiles"

usage() {
    cat <<EOF
Usage: $(basename "$0") [--build-dir <dir>] [--release] [--generator <name>]

Configures and builds the standalone macOS Steam wrappers:
- libmillennium_bootstrap.dylib
- libmillennium_child_hook.dylib
- steam_osx

This only builds the wrapper/bootstrap subproject. Build the top-level project
separately to produce libmillennium.dylib and libmillennium_hhx64.dylib.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ $# -ge 2 ] || {
                printf "Missing value for --build-dir\n" >&2
                exit 1
            }
            BUILD_DIR="$2"
            shift 2
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --generator)
            [ $# -ge 2 ] || {
                printf "Missing value for --generator\n" >&2
                exit 1
            }
            GENERATOR="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf "Unknown argument: %s\n" "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

cmake -S "${SOURCE_DIR}" \
    -B "${BUILD_DIR}" \
    -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DMILLENNIUM_REPO_ROOT="${REPO_ROOT}" \
    -DMILLENNIUM_OUTPUT_NAME=libmillennium.dylib \
    -DMILLENNIUM_BOOTLOADER_OUTPUT_NAME=libmillennium_bootstrap.dylib \
    -DMILLENNIUM_HOOK_HELPER_OUTPUT_NAME=libmillennium_hhx64.dylib \
    -DMILLENNIUM_CHILD_HOOK_OUTPUT_NAME=libmillennium_child_hook.dylib

cmake --build "${BUILD_DIR}"
