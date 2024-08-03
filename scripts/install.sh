#!/usr/bin/env sh

# This file is part of the Millennium project.
# This script is used to install Millennium on a Windows machine.
# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/install.sh
# Copyright (c) 2024 Millennium
set -e

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
    echo "$1"
}

case $(uname -sm) in
    "Linux x86_64") target="linux-x86_64" ;;
    *) log "Unsupported platform $(uname -sm). x86_64 is the only available platform."; exit ;;
esac

# check for dependencies
command -v curl >/dev/null || { log "curl isn't installed!" >&2; exit 1; }
command -v tar >/dev/null || { log "tar isn't installed!" >&2; exit 1; }
command -v grep >/dev/null || { log "grep isn't installed!" >&2; exit 1; }

# download uri
releases_uri="https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"
if [ -z "$tag" ]; then
    tag=$(curl -LsH 'Accept: application/vnd.github.v3+json' "$releases_uri" | jq -r '[.[] | select(.prerelease == false)] | .[0].tag_name')
fi

tag=${tag#v}

log "FETCHING Version $tag"

download_uri=https://github.com/SteamClientHomebrew/Millennium/releases/download/v$tag/millennium-v$tag-$target.tar.gz

# locations
millennium_install="$HOME/.millennium"
exe="$millennium_install/ext/bin/millennium"
bin_dir="$millennium_install/ext/bin"
tar="$millennium_install/millennium.tar.gz"

# installing
[ ! -d "$millennium_install" ] && log "CREATING $millennium_install" && mkdir -p "$millennium_install"

log "DOWNLOADING $download_uri"
curl --fail --location --progress-bar --output "$tar" "$download_uri"

log "EXTRACTING $tar"
tar xzf "$tar" -C "$millennium_install"

log "SETTING EXECUTABLE PERMISSIONS TO $exe"
chmod +x "$exe"
chmod +x "$millennium_install/user32"

log "REMOVING $tar"
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
        log "CREATING $shellrc"
        touch "$shellrc"
    fi

    # Still checking again, in case touch command failed
    if [ -f "$shellrc" ]; then
        if ! grep -q "$bin_dir" "$shellrc"; then
            log "APPENDING $bin_dir to PATH in $shellrc"
            if ! endswith_newline "$shellrc"; then
                echo >> "$shellrc"
            fi
            echo "${2:-$path}" >> "$shellrc"
            export PATH="$bin_dir:$PATH"
        else
            log "millennium path already set in $shellrc, continuing..."
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

echo
log "Millennium@v$tag was installed successfully to $millennium_install"
log "Run 'millennium --help' to get started"
