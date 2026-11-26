#!/bin/bash
# Build and ad-hoc sign the macOS `millennium` command. See README.md.
set -euo pipefail
cd "$(dirname "$0")"

clang -arch arm64 -O2 -Wall -Wextra -fobjc-arc -framework AppKit -o millennium millennium.c native.m
codesign --force --entitlements millennium.entitlements -s - millennium
echo "built + signed millennium"
