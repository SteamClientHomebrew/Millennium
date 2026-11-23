#!/bin/bash
# Build the macOS `millennium` command, ad-hoc signed with cs.debugger (which
# `millennium load` needs to take the task port). Users self-sign the same way
# on install, so no Apple account is involved.
set -euo pipefail
cd "$(dirname "$0")"

clang -arch arm64 -O2 -Wall -Wextra -o millennium millennium.c
codesign --force --entitlements millennium.entitlements -s - millennium
echo "built + signed millennium"
