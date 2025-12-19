#!/usr/bin/env bash

# ==================================================
#   _____ _ _ _             _
#  |     |_| | |___ ___ ___|_|_ _ _____
#  | | | | | | | -_|   |   | | | |     |
#  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
#
# ==================================================

# Copyright (c) 2025 Project Millennium

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/install.sh

readonly GITHUB_ACCOUNT="SteamClientHomebrew/Millennium"
readonly RELEASES_URI="https://api.github.com/repos/${GITHUB_ACCOUNT}/releases"
readonly DOWNLOAD_URI="https://github.com/${GITHUB_ACCOUNT}/releases/download"
readonly INSTALL_DIR="/tmp/millennium"
DRY_RUN=0

log() { printf "%b\n" "$1"; }
is_root() { [ "$(id -u)" -eq 0 ]; }
format_size() {
    echo "$1" | awk '{ split("B KB MB GB TB PB", v); s=1; while ($1 > 1024) { $1 /= 1024; s++ } printf "%.2f %s\n", $1, v[s] }'
}

verify_platform() {
    case $(uname -sm) in
        "Linux x86_64") echo "linux-x86_64" ;;
        *) log "Unsupported platform $(uname -sm). x86_64 is the only available platform."; exit 1 ;;
    esac
}

check_dependencies() {
    log "resolving dependencies..."
    for cmd in curl tar jq sudo; do
        command -v "$cmd" >/dev/null || {
            log "$cmd isn't installed. Install it from your package manager." >&2
            exit 1
        }
    done
}

fetch_release_info() {
    local page=1 per_page=100 response tag size

    while :; do
        response=$(curl -fsSL \
            -H 'Accept: application/vnd.github.v3+json' \
            "$RELEASES_URI?per_page=$per_page&page=$page") || return 1

        [ "$(echo "$response" | jq 'length')" -eq 0 ] && break

        tag=$(echo "$response" | jq -r '
            .[] | select(.prerelease == false) | .tag_name
        ' | head -n1)

        if [ -n "$tag" ] && [ "$tag" != "null" ]; then
            size=$(echo "$response" | jq -r "
                .[] 
                | select(.tag_name == \"$tag\") 
                | .assets[] 
                | select(.name == \"millennium-v${tag#v}-$target.tar.gz\") 
                | .size
            ")
            echo "${tag#v}:${size:-0}"
            return 0
        fi

        page=$((page + 1))
    done

    log "No non-prerelease releases found."
    return 1
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
    if ! curl --fail --location --output "$dest" "$url"; then
        log "Download failed for $url"
        return 1
    fi
}

extract_package() {
    local tar_file="$1"
    local extract_dir="$2"
    mkdir -p "$extract_dir"
    tar xzf "$tar_file" -C "$extract_dir"
}

install_millennium() {
    local extract_path="$1"

    if [ "$DRY_RUN" -eq 0 ]; then
        sudo cp -r "$extract_path"/* / || true
    else
        log "[DRY RUN] Would copy files from $extract_path to /"
    fi
}

post_install() {
    sudo chmod +x /opt/python-i686-3.11.8/bin/python3.11

    log "installing for '$USER'"

    beta_file="$HOME/.steam/steam/package/beta"
    target="$HOME/.steam/steam/ubuntu12_32/libXtst.so.6"

    # make sure to force steam stable for the first install.
    # if the user wants beta they can set it after install.
    # normally its fine, but many users forget they have it enabled on first install,
    # and then face issues.
    if [ -f "$beta_file" ]; then
        log "removing beta '$(cat "$beta_file")' in favor for stable."
        rm "$beta_file"
    fi

    # create a symlink for millenniums preload bootstrap.
    [ -d "$HOME/.steam/steam/ubuntu12_32" ] && ln -sf /usr/lib/millennium/libmillennium_bootstrap_86x.so "$target"
}

cleanup() {
    local dir="$1"
    log "cleaning up..."
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

    if is_root; then
        log "Do not run this script as root!"
        log "aborting installation..."
        exit
    fi

    target=$(verify_platform)
    check_dependencies

    release_info=$(fetch_release_info)
    tag="${release_info%%:*}"
    size=$(format_size "${release_info##*:}")

    install_size_uri="${DOWNLOAD_URI}/v$tag/millennium-v${tag}-$target.installsize"
    download_uri="${DOWNLOAD_URI}/v$tag/millennium-v${tag}-$target.tar.gz"
    sha256_uri="${DOWNLOAD_URI}/v$tag/millennium-v${tag}-$target.sha256"

    sha256digest=$(curl -sL $sha256_uri)
    installed_size=$(format_size "$(curl -sL $install_size_uri)")

    log "\nPackages (1) millennium@$tag-x86_64\n"
    log "Total Download Size:  $(printf "%10s\n" "$size")"
    log "Total Installed Size: $(printf "%10s\n" "$installed_size")"

    confirm_installation
    log "receiving packages..."

    install_dir="${DRY_RUN:+./dry-run}"
    install_dir="${install_dir:-$INSTALL_DIR}"
    extract_path="$install_dir/files"
    tar_file="$install_dir/millennium-v${tag}-$target.tar.gz"

    rm -rf "$install_dir"
    mkdir -p "$install_dir"

    log "(1/4) Downloading millennium-v${tag}-$target.tar.gz..."
    download_package "$download_uri" "$tar_file"
    log "(2/4) Verifying checksums..."
    # use sub-shell to prevent actually changing the working directory
    if (cd "$install_dir" && echo "$sha256digest" | sha256sum -c --status); then
        echo -ne "\033[1A"
        log "(2/4) Verifying checksums... OK"
    else
        log "(2/4) Verifying checksums... FAILED"
    fi
    log "(3/4) Unpacking millennium-v${tag}-$target.tar.gz..."
    extract_package "$tar_file" "$extract_path"
    log "(4/4) Installing millennium..."
    install_millennium "$extract_path"

    log ":: Running post-install scripts..."
    log "(1/1) Setting up shared object preloader hook..."
    post_install

    cleanup "$install_dir"

    log "done.\n"
    log "You can now start Steam."
    log "https://docs.steambrew.app/users/installing#post-installation."
}

main "$@"
