#!/bin/bash
destinationBase="${1:-/home/runner/env/ext/data/assets}"
echo "Copying assets to $destinationBase"

declare -A paths=(
    ["./assets/.millennium/Dist"]="$destinationBase/.millennium/Dist"
    ["./assets/core"]="$destinationBase/core"
    ["./assets/pipx"]="$destinationBase/pipx"
    ["./assets/requirements.txt"]="$destinationBase/requirements.txt"
    ["./assets/plugin.json"]="$destinationBase/plugin.json"
)

for source in "${!paths[@]}"; do
    destination="${paths[$source]}"
    destinationDir="$(dirname "$destination")"
    if [ ! -d "$destinationDir" ]; then
        mkdir -p "$destinationDir"
    fi
    cp -r "$source" "$destination"
done
