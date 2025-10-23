#!/usr/bin/env sh
# This file is part of the Millennium project.
# This script is used to install Millennium on a Linux machine.
# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/install.sh
# Copyright (c) 2025 Millennium

set -e

readonly RELEASES_URI="https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"
readonly INSTALL_DIR="/tmp/millennium"
DRY_RUN=0

log() {
    printf "%b\n" "$1"
}

# root is required to install Millennium as it installs into /usr/lib
is_root() {
    [ "$(id -u)" -eq 0 ]
}

check_permissions() {
    if [ "$DRY_RUN" -eq 0 ] && ! is_root; then
        log "Insufficient permissions. Please run the script as root."
        exit 1
    fi
    [ "$DRY_RUN" -eq 1 ] && log "[DRY RUN] No changes will be made to the system."
}

verify_platform() {
    case $(uname -sm) in
        "Linux x86_64") echo "linux-x86_64" ;;
        *) log "Unsupported platform $(uname -sm). x86_64 is the only available platform."; exit 1 ;;
    esac
}

check_dependencies() {
    log "resolving dependencies..."
    for cmd in curl tar jq; do
        command -v "$cmd" >/dev/null || {
            log "$cmd isn't installed! Install it from your package manager." >&2
            exit 1
        }
    done
}

fetch_release_info() {
    local response tag size
    response=$(curl -LsH 'Accept: application/vnd.github.v3+json' "$RELEASES_URI")
    tag=$(echo "$response" | jq -r '.[0].tag_name')
    size=$(echo "$response" | jq -r '.[0].assets | .[0].size')
    echo "${tag#v}:${size:-0}"
}

format_size() {
    echo "$1" | awk '{ split("B KB MB GB TB PB", v); s=1; while ($1 > 1024) { $1 /= 1024; s++ } printf "%.2f %s\n", $1, v[s] }'
}

confirm_installation() {
    echo -e "\n:: Proceed with installation? [Y/n] \c"
    read -r proceed </dev/tty
    case "$proceed" in
        [Nn]*) exit 1 ;;
        *) return 0 ;;
    esac
}

download_package() {
    local url="$1"
    local dest="$2"
    log "receiving packages..."
    curl --fail --location --output "$dest" "$url" --silent
}

extract_package() {
    local tar_file="$1"
    local extract_dir="$2"
    log "deflating received packages..."
    mkdir -p "$extract_dir"
    tar xzf "$tar_file" -C "$extract_dir"
}

install_files() {
    local extract_path="$1"
    local folder_size

    folder_size=$(du -sb "$extract_path" | awk '{print $1}' | numfmt --to=iec-i --suffix=B --padding=7 | sed 's/\([0-9]\)\([A-Za-z]\)/\1 \2/')
    log "\nTotal Install Size: $folder_size"

    if [ "$DRY_RUN" -eq 0 ]; then
        cp -r "$extract_path"/* / || true
        chmod +x /opt/python-i686-3.11.8/bin/python3.11
    else
        log "[DRY RUN] Would copy files from $extract_path to /"
        log "[DRY RUN] Would chmod +x /opt/python-i686-3.11.8/bin/python3.11"
    fi
}

cleanup() {
    local dir="$1"
    log "cleaning up packages..."
    rm -rf "$dir"
}

main() {
    local target release_info tag size download_uri install_dir extract_path tar_file

    # Parse arguments
    for arg in "$@"; do
        case $arg in
            --dry-run) DRY_RUN=1; shift ;;
        esac
    done

    check_permissions
    target=$(verify_platform)
    check_dependencies

    release_info=$(fetch_release_info)
    tag="${release_info%%:*}"
    size="${release_info##*:}"

    download_uri="https://github.com/SteamClientHomebrew/Millennium/releases/download/v$tag/millennium-v${tag%-*}-$target.tar.gz"
    size=$(format_size "$size")

    log "\nPackages (1) millennium@$tag-x86_64"
    log "\nTotal Download Size: $size"
    log "Retreiving packages..."

    confirm_installation

    install_dir="${DRY_RUN:+./dry-run}"
    install_dir="${install_dir:-$INSTALL_DIR}"
    extract_path="$install_dir/files"
    tar_file="$install_dir/millennium.tar.gz"

    rm -rf "$install_dir"
    mkdir -p "$install_dir"

    download_package "$download_uri" "$tar_file"
    extract_package "$tar_file" "$extract_path"
    install_files "$extract_path"
    cleanup "$install_dir"

    log "done."
    log "\nhttps://docs.steambrew.app/users/installing#post-installation.\nYou can now start Steam.\n\nNOTE: Your base installation of Steam has not been modified, this is simply an extension."
}

main "$@"
