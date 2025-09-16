#!/bin/bash
destinationBase="${1:-/home/runner/env/ext/data/assets}"
echo "Copying assets to $destinationBase"

declare -A paths=(
    ["./assets/pipx"]="$destinationBase/pipx"
)

for source in "${!paths[@]}"; do
    destination="${paths[$source]}"
    destinationDir="$(dirname "$destination")"
    if [ ! -d "$destinationDir" ]; then
        mkdir -p "$destinationDir"
    fi
    cp -r "$source" "$destination"
done
