#!/bin/bash
# `millennium setup` stages the built runtime into a content-hashed release and
# points `current` at it, under a redirected HOME.
set -eu

build=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export HOME="$work"
runtime="$work/Library/Application Support/Millennium/runtime"
cur="$runtime/current"

"$build/millennium" setup --from "$build"
test -L "$cur"
test -x "$cur/millennium"
test -f "$cur/libmillennium.dylib"
test -f "$cur/libmillennium_bootstrap.dylib"
test -f "$cur/libmillennium_child_hook.dylib"
test -f "$cur/libmillennium_hhx64.dylib"
"$cur"/libmillennium_luavm_* --version

# Re-staging the same build is idempotent: same release, nothing new.
before=$(readlink "$cur")
"$build/millennium" setup --from "$build"
test "$(readlink "$cur")" = "$before"
test "$(ls -d "$runtime"/release-* | wc -l)" -eq 1

# uninstall removes the staged runtime.
"$build/millennium" uninstall
test ! -e "$cur"
test ! -e "$runtime"
