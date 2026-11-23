#!/bin/bash
# `millennium setup` stages the built runtime under a redirected HOME.
set -eu

build=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export HOME="$work"
runtime="$work/Library/Application Support/Millennium/runtime"

"$build/millennium" setup --from "$build"
test -x "$runtime/millennium"
test -f "$runtime/libmillennium.dylib"
test -f "$runtime/libmillennium_bootstrap.dylib"
test -f "$runtime/libmillennium_child_hook.dylib"
test -f "$runtime/libmillennium_hhx64.dylib"
"$runtime"/libmillennium_luavm_* --version

# Re-running is idempotent.
"$build/millennium" setup --from "$build"
test -x "$runtime/millennium"

# uninstall clears the payload.
"$build/millennium" uninstall
test ! -e "$runtime/libmillennium_bootstrap.dylib"
