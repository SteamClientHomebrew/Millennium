#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STARLIGHT="$REPO_ROOT/starlight"

PROFILE="${1:-debug}"
CARGO_ARGS=()
if [[ "$PROFILE" == "release" ]]; then
    CARGO_ARGS+=(--release)
fi

cargo build --manifest-path "$STARLIGHT/Cargo.toml" "${CARGO_ARGS[@]}" 2>&1 | grep -E "^error|^warning\[" || true

mkdir -p "$STARLIGHT/npm/binaries"

PLATFORM="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"
[[ "$ARCH" == "x86_64" ]] && ARCH="x64"
[[ "$ARCH" == "aarch64" ]] && ARCH="arm64"

DEST="$STARLIGHT/npm/binaries/starlight-${PLATFORM}-${ARCH}"
cp "$STARLIGHT/target/$PROFILE/starlight" "$DEST"

bun link --cwd "$STARLIGHT/npm" &>/dev/null
