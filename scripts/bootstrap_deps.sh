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

DEPS_DIR="${PWD}/deps"
INCLUDE_WINDOWS_DEPS=0
DISABLE_PROXY=0

log() { printf "%b\n" "$1"; }

fail() {
    log "$1" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [--deps-dir <dir>] [--include-windows] [--no-proxy] [--help]

Clones the third-party repositories that Millennium expects under ./deps so the
top-level CMake build can use local dependencies instead of FetchContent.

This is intended for development checkouts and mirrors the dependency revisions
declared in the CMake build files.
EOF
}

ensure_git() {
    command -v git >/dev/null 2>&1 || fail "git is required to bootstrap Millennium dependencies."
}

log_proxy_mode() {
    local configured_proxy=""

    configured_proxy="${HTTPS_PROXY:-${https_proxy:-${ALL_PROXY:-${all_proxy:-${HTTP_PROXY:-${http_proxy:-}}}}}}"

    if [ "${DISABLE_PROXY}" -eq 1 ]; then
        log "Cloning dependencies with proxy environment variables disabled."
    elif [ -n "${configured_proxy}" ]; then
        log "Using inherited git proxy settings (${configured_proxy}). Pass --no-proxy to bypass them for this run."
    fi
}

run_git_clone() {
    if [ "${DISABLE_PROXY}" -eq 1 ]; then
        env \
            -u http_proxy \
            -u https_proxy \
            -u HTTP_PROXY \
            -u HTTPS_PROXY \
            -u all_proxy \
            -u ALL_PROXY \
            git clone "$@"
        return 0
    fi

    git clone "$@"
}

clone_dep() {
    local name="$1"
    local url="$2"
    local ref="$3"
    local target="${DEPS_DIR}/${name}"

    if [ -d "${target}/.git" ]; then
        log "Skipping ${name}, existing checkout found at ${target}"
        return 0
    fi

    if [ -e "${target}" ]; then
        fail "Refusing to overwrite non-git path: ${target}"
    fi

    log "Cloning ${name} (${ref})..."
    run_git_clone --depth 1 --branch "${ref}" "${url}" "${target}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --deps-dir)
            [ $# -ge 2 ] || fail "Missing value for --deps-dir"
            DEPS_DIR="$2"
            shift 2
            ;;
        --include-windows)
            INCLUDE_WINDOWS_DEPS=1
            shift
            ;;
        --no-proxy)
            DISABLE_PROXY=1
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

ensure_git
log_proxy_mode
mkdir -p "${DEPS_DIR}"

# Mirrors scripts/cmake/bootstrap_deps.cmake and src/hhx64/CMakeLists.txt.
clone_dep "zlib"        "https://github.com/zlib-ng/zlib-ng"            "2.2.5"
clone_dep "luajit"      "https://github.com/SteamClientHomebrew/LuaJIT" "v2.1"
clone_dep "luajson"     "https://github.com/SteamClientHomebrew/LuaJSON" "master"
clone_dep "mini"        "https://github.com/metayeti/mINI"              "0.9.18"
clone_dep "websocketpp" "https://github.com/zaphoyd/websocketpp"        "0.8.2"
clone_dep "fmt"         "https://github.com/fmtlib/fmt"                 "12.0.0"
clone_dep "json"        "https://github.com/nlohmann/json"              "v3.12.0"
clone_dep "libgit2"     "https://github.com/libgit2/libgit2"            "v1.9.1"
clone_dep "minizip"     "https://github.com/zlib-ng/minizip-ng"         "4.0.10"
clone_dep "curl"        "https://github.com/curl/curl"                  "curl-8_13_0"
clone_dep "incbin"      "https://github.com/graphitemaster/incbin"      "main"
clone_dep "asio"        "https://github.com/chriskohlhoff/asio"         "asio-1-30-0"
clone_dep "abseil"      "https://github.com/abseil/abseil-cpp"          "20240722.0"
clone_dep "re2"         "https://github.com/google/re2"                 "2025-11-05"

if [ "${INCLUDE_WINDOWS_DEPS}" -eq 1 ]; then
    clone_dep "minhook" "https://github.com/TsudaKageyu/minhook" "v1.3.4"
fi

log "Millennium dependency bootstrap complete in ${DEPS_DIR}"
