#!/bin/bash

cd "/home/runner/Millennium"
destinationBase="/home/runner/env/ext/data/assets"

declare -A paths=(
    ["./assets/.millennium/Dist/index.js"]="$destinationBase/.millennium/Dist/index.js"
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
