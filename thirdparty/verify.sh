#!/usr/bin/env bash
# ==================================================
#   _____ _ _ _             _
#  |     |_| | |___ ___ ___|_|_ _ _____
#  | | | | | | | -_|   |   | | | |     |
#  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
#
# ==================================================
#
# Copyright (c) 2026 Project Millennium
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Verifies vendored tarballs against their upstream sources.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PASS=0
FAIL=0
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

check() {
    local file="$1"
    local url="$2"

    if [[ ! -f "$file" ]]; then
        echo "MISSING  $file"
        FAIL=$((FAIL + 1))
        return
    fi

    printf "checking %-40s ... " "$file"
    curl -fsSL -o "$TMPDIR/$file" "$url"

    local committed upstream
    committed="$(shasum -a 256 "$file"        | awk '{print $1}')"
    upstream="$(shasum  -a 256 "$TMPDIR/$file" | awk '{print $1}')"

    if [[ "$committed" == "$upstream" ]]; then
        echo "OK"
        PASS=$((PASS + 1))
    else
        echo "MISMATCH"
        echo "  committed: $committed"
        echo "  upstream:  $upstream"
        FAIL=$((FAIL + 1))
    fi
}

check zlib-2.2.5.tar.gz            https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.2.5.tar.gz
check luajit-89550023.tar.gz       https://github.com/SteamClientHomebrew/LuaJIT/archive/89550023569c3e195e75e12951c067fe5591e0d2.tar.gz
check lua_cjson-0c1fabf0.tar.gz    https://github.com/SteamClientHomebrew/LuaJSON/archive/0c1fabf07c42f3907287d1e4f729e0620c1fe6fd.tar.gz
check websocketpp-0.8.2.tar.gz     https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz
check fmt-12.0.0.tar.gz            https://github.com/fmtlib/fmt/archive/refs/tags/12.0.0.tar.gz
check nlohmann_json-v3.12.0.tar.gz https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
check libgit2-v1.9.1.tar.gz        https://github.com/libgit2/libgit2/archive/refs/tags/v1.9.1.tar.gz
check minizip_ng-4.0.10.tar.gz     https://github.com/zlib-ng/minizip-ng/archive/refs/tags/4.0.10.tar.gz
check curl-8_13_0.tar.gz           https://github.com/curl/curl/archive/refs/tags/curl-8_13_0.tar.gz
check incbin-22061f51.tar.gz       https://github.com/graphitemaster/incbin/archive/22061f51fe9f2f35f061f85c2b217b55dd75310d.tar.gz
check asio-1-30-0.tar.gz           https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-0.tar.gz

echo ""
echo "$PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
