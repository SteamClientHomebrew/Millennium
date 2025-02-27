#!/usr/bin/env sh

# This file is part of the Millennium project.
# This script is used to install Millennium on a Linux machine.
# https://github.com/shdwmtr/millennium/blob/main/scripts/install.sh
# Copyright (c) 2025 Millennium
set -e

BOLD_RED="\033[1;31m"
BOLD_PINK="\033[1;35m"
BOLD_GREEN="\033[1;32m"
BOLD_YELLOW="\033[1;33m"
BOLD_BLUE="\033[1;34m"
BOLD_MAGENTA="\033[1;35m"
BOLD_CYAN="\033[1;36m"
BOLD_WHITE="\033[1;37m"
RESET="\033[0m"

for arg in "$@"; do
    shift
    case "$arg" in
        "--root") override_root=1 ;;
        *)
        if echo "$arg" | grep -qv "^-"; then
            tag="$arg"
        else
            echo "Invalid option $arg" >&2
            exit 1
        fi
    esac
done

is_root() {
    [ "$(id -u)" -ne 0 ]
}

if ! is_root && [ "${override_root:-0}" -eq 0 ]; then
    echo "The script was ran under sudo or as root. The script will now exit"
    echo "If you hadn't intended to do this, please execute the script without root access to avoid problems with spicetify"
    echo "To override this behavior, pass the '--root' parameter to this script"
    exit
fi


log() {
    echo -e "$1"
}

case $(uname -sm) in
    "Linux x86_64") target="linux-x86_64" ;;
    *) log "Unsupported platform $(uname -sm). x86_64 is the only available platform."; exit ;;
esac

# check for dependencies
command -v curl >/dev/null || { log "curl isn't installed!" >&2; exit 1; }
command -v tar >/dev/null || { log "tar isn't installed!" >&2; exit 1; }
command -v grep >/dev/null || { log "grep isn't installed!" >&2; exit 1; }
command -v jq >/dev/null || { log "jq isn't installed!" >&2; exit 1; }

# download uri
releases_uri="https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"
if [ -z "$tag" ]; then
    tag=$(curl -LsH 'Accept: application/vnd.github.v3+json' "$releases_uri" | jq -r '[.[] | select(.prerelease == false)] | .[0].tag_name')
    # get the bytes size of the release assets
    size=$(curl -LsH 'Accept: application/vnd.github.v3+json' "$releases_uri" | jq -r '[.[] | select(.prerelease == false)] | .[0].assets | .[0].size')
fi

tag=${tag#v}
size=${size:-0}

# convert bytes to human readable format
size=$(echo $size | awk '{ split("B KB MB GB TB PB", v); s=1; while ($1 > 1024) { $1 /= 1024; s++ } printf "%.2f %s\n", $1, v[s] }')

log "${BOLD_PINK}++${RESET} Installing Packages (Linux) Millennium@$tag"
log "\n${BOLD_PINK}::${RESET} Total Download Size: $size"
log "${BOLD_PINK}::${RESET} Retreiving packages..."

download_uri=https://github.com/SteamClientHomebrew/Millennium/releases/download/v$tag/millennium-v$tag-$target.tar.gz

# locations
millennium_install="$HOME/.millennium"
exe="$millennium_install/ext/bin/millennium"
bin_dir="$millennium_install/ext/bin"
tar="$millennium_install/millennium.tar.gz"

# installing
[ ! -d "$millennium_install" ] && log "${BOLD_PINK}++${RESET} Creating $millennium_install" && mkdir -p "$millennium_install"

log "\n${BOLD_PINK}(1/2)${RESET} Downloading millennium-v$tag-$target..."

# Download the file with custom progress
curl --fail --location --output "$tar" "$download_uri" --silent

log "${BOLD_PINK}(2/2)${RESET} Unpacking Assets..."

tar xzf "$tar" -C "$millennium_install"

uncompressed_size=$(echo $(gzip -l "$tar" | awk 'NR==2 {print $2}') \
    | awk '{ split("B KB MB GB TB PB", v); s=1; while ($1 > 1024) { $1 /= 1024; s++ } printf "%.2f %s\n", $1, v[s] }')


log "\n${BOLD_PINK}::${RESET} Total Installed Size: $uncompressed_size"

log "${BOLD_PINK}::${RESET} Setting up permissions..."

chmod +x "$exe"
chmod +x "$millennium_install/start.sh"
chmod +x "$HOME/.millennium/ext/data/cache/bin/python3.11"

log "\n${BOLD_PINK}++${RESET} Cleaning up installer packages..."
rm "$tar"

notfound() {
    cat << EOINFO
Manually add the directory to your \$PATH through your shell profile
export millennium_install="$millennium_install"
export PATH="\$PATH:$millennium_install"
EOINFO
}

endswith_newline() {
    [ "$(od -An -c "$1" | tail -1 | grep -o '.$')" = "\n" ]
}

check() {
    path="export PATH=\$PATH:$bin_dir"
    shellrc=$HOME/$1

    if [ "$1" = ".zshrc" ] && [ -n "${ZDOTDIR}" ]; then
        shellrc=$ZDOTDIR/$1
    fi

    # Create shellrc if it doesn't exist
    if ! [ -f "$shellrc" ]; then
        log "${BOLD_PINK}++${RESET} Creating $shellrc"
        touch "$shellrc"
    fi

    # Still checking again, in case touch command failed
    if [ -f "$shellrc" ]; then
        if ! grep -q "$bin_dir" "$shellrc"; then
            log "${BOLD_PINK}++${RESET} Appending $bin_dir to PATH in $shellrc"
            if ! endswith_newline "$shellrc"; then
                echo >> "$shellrc"
            fi
            echo "${2:-$path}" >> "$shellrc"
            export PATH="$bin_dir:$PATH"
        fi
    else
        notfound
    fi
}

case $SHELL in
    *zsh) check ".zshrc" ;;
    *bash)
        [ -f "$HOME/.bashrc" ] && check ".bashrc"
        [ -f "$HOME/.bash_profile" ] && check ".bash_profile"
    ;;
    *fish) check ".config/fish/config.fish" "fish_add_path $bin_dir" ;;
    *) notfound ;;
esac


log "${BOLD_PINK}++${RESET} Millennium@v$tag was installed successfully to $millennium_install"

log "\n${BOLD_PINK}::${RESET} To start Millennium along side Steam, run ~/.millennium/start.sh\n   Steam MUST be started from this script to work properly with Millennium.\n   Your base installation of Steam has not been modified, this is simply an extension."
