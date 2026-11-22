#!/bin/bash
set -eu

installer=$1
build=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

runtime="$work/Application Support/runtime"
app="$work/Steam Millennium.app"
opts=(--update-runtime --runtime-install-root "$runtime" --app-bundle "$app")

# macOS mv follows a symlink to a directory unless passed -h.
mkdir -p "$runtime/older"
ln -s older "$runtime/current"
"$installer" "${opts[@]}" --build-dir "$build"
release=$(readlink "$runtime/current")
test "$release" != older
test -d "$runtime/older"
test -x "$runtime/current/steam_osx"
"$runtime/current/libmillennium_luavm_x86" --version
test ! -e "$app"

"$installer" "${opts[@]}" --build-dir "$build"
test "$(readlink "$runtime/current")" = "$release"

mkdir "$work/incomplete"
cp "$build/libmillennium.dylib" "$work/incomplete/"
if "$installer" "${opts[@]}" --build-dir "$work/incomplete"; then
    echo "Installed an incomplete runtime" >&2
    exit 1
fi
test "$(readlink "$runtime/current")" = "$release"
