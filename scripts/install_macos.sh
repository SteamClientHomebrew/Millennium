#!/usr/bin/env bash

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly PROXY_SOURCE="${REPO_ROOT}/src/boot/macos/src/tier0_proxy.cc"
readonly DEFAULT_STEAM_DIR="${HOME}/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS"
readonly DEFAULT_WRAPPER_BUILD_DIR="${REPO_ROOT}/build/osx-debug-make/src/boot/macos"
readonly DEFAULT_RUNTIME_SEARCH_DIRS=(
    "${REPO_ROOT}/build/osx-debug-make/src"
    "${REPO_ROOT}/build/osx-release-make/src"
    "${REPO_ROOT}/build/osx-debug/src"
    "${REPO_ROOT}/build/osx-release/src"
    "${REPO_ROOT}/build"
)
readonly REQUIRED_RUNTIME_ARTIFACTS=(
    "libmillennium.dylib"
    "libmillennium_hhx64.dylib"
)
readonly OPTIONAL_RUNTIME_ARTIFACTS=(
    "libmillennium_child_hook.dylib"
)

readonly TIER0_NAME="libtier0_s.dylib"
readonly TIER0_REAL_NAME="libtier0_s.real.dylib"
readonly CEF_MARKER_NAME=".cef-enable-remote-debugging"
readonly STEAM_CFG_NAME="Steam.cfg"
readonly STEAM_CFG_BACKUP_NAME="Steam.cfg.millennium.backup"
readonly STEAM_CFG_STATE_NAME="Steam.cfg.millennium.state"
readonly STEAM_CFG_MANAGED_MARKER="# Added by Millennium install_macos.sh"
readonly STEAM_CFG_INHIBIT_ALL_LINE="BootStrapperInhibitAll=enable"
readonly ICON_SOURCE_PATH="${REPO_ROOT}/src/boot/macos/AppIcon.icns"
readonly ICON_BACKUP_SUFFIX=".millennium.backup"
readonly BUNDLE_ICON_OVERRIDE_BACKUP_NAME=".millennium.bundle.icon-override.backup"
readonly BUNDLE_FINDERINFO_BACKUP_NAME=".millennium.bundle.finderinfo.hex"

MODE="install"
LEGACY_BUILD_DIR=""
RUNTIME_BUILD_DIR=""
WRAPPER_BUILD_DIR="${DEFAULT_WRAPPER_BUILD_DIR}"
STEAM_DIR="${DEFAULT_STEAM_DIR}"
DRY_RUN=0
CONFIGURE_STEAM_CFG_INHIBIT_ALL=0
CONFIGURE_ICON_REPLACEMENT=0
RUNTIME_SEARCH_DIRS=()

log() { printf "%b\n" "$1"; }

fail() {
    log "$1" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [--build-dir <dir>] [--runtime-build-dir <dir>] [--wrapper-build-dir <dir>] [--steam-dir <dir>] [--dry-run] [--repair] [--status] [--uninstall] [--steam-cfg-inhibit-all] [--restore-steam-cfg] [--replace-icon] [--restore-icon]

Installs a passive macOS tier0 proxy without replacing steam_osx:
1. Backs up Steam's ${TIER0_NAME} to ${TIER0_REAL_NAME}
2. Compiles a local reexport proxy from ${PROXY_SOURCE}
3. Installs Millennium runtime dylibs beside Steam binaries

Modes:
  (default)   install   Install proxy + runtime artifacts
  --repair              Re-apply proxy after Steam updates (keeps existing runtime files when build artifacts are absent)
  --status              Print current proxy/runtime state
  --uninstall           Restore original ${TIER0_NAME} from ${TIER0_REAL_NAME}
  --restore-steam-cfg   Restore Steam.cfg to the state captured before --steam-cfg-inhibit-all was first applied
  --restore-icon        Restore Steam icon from the backup created by --replace-icon

Notes:
- This modifies Steam's tracked files and may be overwritten by Steam updates.
- Run --repair after Steam updates to self-heal.
- The tier0 proxy creates ${CEF_MARKER_NAME} on each Steam launch so Millennium can attach frontend injection.
- Use --steam-cfg-inhibit-all to write ${STEAM_CFG_INHIBIT_ALL_LINE} into Steam.cfg.
- Use --replace-icon to replace Steam's icon with ${ICON_SOURCE_PATH} (backup is auto-created).
- --replace-icon also clears bundle-level Icon\r/FinderInfo overrides that can mask Steam.icns.
- The above flag allows direct Steam.app launches to keep the proxy, but Steam updates are suppressed while enabled.
EOF
}

run_cmd() {
    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] $*"
        return 0
    fi

    "$@"
}

ensure_steam_closed() {
    if pgrep -x "steam_osx" >/dev/null 2>&1 || pgrep -f "/Steam Helper" >/dev/null 2>&1; then
        fail "Quit Steam before modifying the runtime bundle."
    fi
}

resolve_steam_contents_dir() {
    (
        cd "${STEAM_DIR}/.." >/dev/null 2>&1 || exit 1
        pwd
    )
}

resolve_steam_bundle_root() {
    (
        cd "${STEAM_DIR}/../.." >/dev/null 2>&1 || exit 1
        pwd
    )
}

backup_and_clear_bundle_icon_override() {
    local bundle_root
    bundle_root=$(resolve_steam_bundle_root) || fail "Failed to resolve Steam bundle root path."

    local icon_override_path="${bundle_root}/"$'Icon\r'
    local icon_override_backup="${bundle_root}/${BUNDLE_ICON_OVERRIDE_BACKUP_NAME}"
    local finderinfo_backup="${bundle_root}/${BUNDLE_FINDERINFO_BACKUP_NAME}"

    if [ -f "${icon_override_path}" ]; then
        if [ ! -f "${icon_override_backup}" ]; then
            run_cmd cp "${icon_override_path}" "${icon_override_backup}"
        fi
        run_cmd rm -f "${icon_override_path}"
    fi

    if xattr -p com.apple.FinderInfo "${bundle_root}" >/dev/null 2>&1; then
        if [ ! -f "${finderinfo_backup}" ]; then
            if [ "${DRY_RUN}" -eq 1 ]; then
                log "[DRY RUN] xattr -px com.apple.FinderInfo ${bundle_root} > ${finderinfo_backup}"
            else
                xattr -px com.apple.FinderInfo "${bundle_root}" > "${finderinfo_backup}"
            fi
        fi
        run_cmd xattr -d com.apple.FinderInfo "${bundle_root}"
    fi
}

restore_bundle_icon_override() {
    local bundle_root
    bundle_root=$(resolve_steam_bundle_root) || fail "Failed to resolve Steam bundle root path."

    local icon_override_path="${bundle_root}/"$'Icon\r'
    local icon_override_backup="${bundle_root}/${BUNDLE_ICON_OVERRIDE_BACKUP_NAME}"
    local finderinfo_backup="${bundle_root}/${BUNDLE_FINDERINFO_BACKUP_NAME}"
    local restored_count=0

    if [ -f "${icon_override_backup}" ]; then
        run_cmd mv -f "${icon_override_backup}" "${icon_override_path}"
        restored_count=$((restored_count + 1))
    fi

    if [ -f "${finderinfo_backup}" ]; then
        if [ "${DRY_RUN}" -eq 1 ]; then
            log "[DRY RUN] xattr -wx com.apple.FinderInfo <hex-from-${finderinfo_backup}> ${bundle_root}"
            run_cmd rm -f "${finderinfo_backup}"
        else
            local hex_value
            hex_value=$(tr -d '[:space:]' < "${finderinfo_backup}" || true)
            if [ -n "${hex_value}" ]; then
                xattr -wx com.apple.FinderInfo "${hex_value}" "${bundle_root}"
            fi
            rm -f "${finderinfo_backup}"
        fi
        restored_count=$((restored_count + 1))
    fi

    if [ "${restored_count}" -gt 0 ]; then
        log "Restored Steam bundle custom icon override metadata (${restored_count} item(s))."
    fi
}

resolve_steam_icon_name() {
    local contents_dir="$1"
    local icon_file="Steam.icns"
    local info_plist=""
    info_plist="${contents_dir}/Info.plist"
    if [ -f "${info_plist}" ]; then
        local configured_icon
        configured_icon=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" "${info_plist}" 2>/dev/null || true)
        if [ -n "${configured_icon}" ]; then
            icon_file="${configured_icon}"
        fi
    fi

    case "${icon_file}" in
        *.icns) ;;
        *) icon_file="${icon_file}.icns" ;;
    esac

    printf "%s\n" "${icon_file}"
}

resolve_steam_icon_paths() {
    local contents_dir
    contents_dir=$(resolve_steam_contents_dir) || return 1
    local icon_name
    icon_name=$(resolve_steam_icon_name "${contents_dir}") || return 1

    local resources_icon="${contents_dir}/Resources/${icon_name}"
    local macos_icon="${contents_dir}/MacOS/${icon_name}"
    local resources_backup="${resources_icon}${ICON_BACKUP_SUFFIX}"
    local macos_backup="${macos_icon}${ICON_BACKUP_SUFFIX}"

    printf "%s\n" "${resources_icon}"
    if [ -f "${macos_icon}" ] || [ -f "${macos_backup}" ]; then
        printf "%s\n" "${macos_icon}"
    fi
}

install_custom_icon() {
    [ -f "${ICON_SOURCE_PATH}" ] || fail "Custom icon source missing: ${ICON_SOURCE_PATH}"
    backup_and_clear_bundle_icon_override

    local icon_targets=()
    while IFS= read -r icon_target; do
        [ -n "${icon_target}" ] || continue
        icon_targets+=("${icon_target}")
    done < <(resolve_steam_icon_paths) || fail "Failed to resolve Steam icon target path."

    [ "${#icon_targets[@]}" -gt 0 ] || fail "Failed to resolve Steam icon target path."
    local replaced_count=0

    for icon_target in "${icon_targets[@]}"; do
        local icon_backup="${icon_target}${ICON_BACKUP_SUFFIX}"
        [ -f "${icon_target}" ] || fail "Steam icon target missing: ${icon_target}"

        if [ ! -f "${icon_backup}" ]; then
            run_cmd cp "${icon_target}" "${icon_backup}"
        fi

        run_cmd cp "${ICON_SOURCE_PATH}" "${icon_target}"
        replaced_count=$((replaced_count + 1))
    done

    log "Installed custom Steam icon from ${ICON_SOURCE_PATH} into ${replaced_count} target(s)."
}

restore_custom_icon() {
    local icon_targets=()
    while IFS= read -r icon_target; do
        [ -n "${icon_target}" ] || continue
        icon_targets+=("${icon_target}")
    done < <(resolve_steam_icon_paths) || fail "Failed to resolve Steam icon target path."

    [ "${#icon_targets[@]}" -gt 0 ] || fail "Failed to resolve Steam icon target path."
    local restored_count=0

    for icon_target in "${icon_targets[@]}"; do
        local icon_backup="${icon_target}${ICON_BACKUP_SUFFIX}"
        if [ -f "${icon_backup}" ]; then
            run_cmd mv -f "${icon_backup}" "${icon_target}"
            restored_count=$((restored_count + 1))
        fi
    done

    if [ "${restored_count}" -gt 0 ]; then
        log "Restored Steam icon from backup in ${restored_count} target(s)."
    else
        log "No Steam icon backup was found for any target."
    fi

    restore_bundle_icon_override
}

find_artifact_optional() {
    local artifact_name="$1"
    shift
    local result=""

    for search_dir in "$@"; do
        [ -n "${search_dir}" ] || continue
        result=$(find "${search_dir}" -type f -name "${artifact_name}" -print -quit 2>/dev/null || true)
        if [ -n "${result}" ]; then
            printf "%s\n" "${result}"
            return 0
        fi
    done

    printf "\n"
    return 0
}

install_file() {
    local source_path="$1"
    local destination_path="$2"

    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] install -m 0755 ${source_path} ${destination_path}"
        return 0
    fi

    install -m 0755 "${source_path}" "${destination_path}"
}

is_tier0_proxy() {
    local target="$1"
    [ -f "${target}" ] || return 1
    nm -gU "${target}" 2>/dev/null | rg -q "millennium_tier0_proxy_marker"
}

strip_managed_steam_cfg_lines() {
    local source_path="$1"
    local output_path="$2"

    if [ ! -f "${source_path}" ]; then
        if [ "${DRY_RUN}" -eq 1 ]; then
            log "[DRY RUN] : > ${output_path}"
            return 0
        fi
        : > "${output_path}"
        return 0
    fi

    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] strip managed Steam.cfg lines from ${source_path} -> ${output_path}"
        return 0
    fi

    awk '
        /^[[:space:]]*BootStrapperInhibitAll[[:space:]]*=/ { next }
        /^# Added by Millennium install_macos\.sh$/ { next }
        /^# Added by Millennium install_macos_tier0\.sh$/ { next }
        { print }
    ' "${source_path}" > "${output_path}"
}

backup_steam_cfg_state_if_needed() {
    local steam_cfg="${STEAM_DIR}/${STEAM_CFG_NAME}"
    local steam_cfg_backup="${STEAM_DIR}/${STEAM_CFG_BACKUP_NAME}"
    local steam_cfg_state="${STEAM_DIR}/${STEAM_CFG_STATE_NAME}"

    if [ -f "${steam_cfg_state}" ]; then
        return 0
    fi

    if [ -f "${steam_cfg}" ]; then
        run_cmd cp "${steam_cfg}" "${steam_cfg_backup}"
        if [ "${DRY_RUN}" -eq 1 ]; then
            log "[DRY RUN] printf 'present\\n' > ${steam_cfg_state}"
            return 0
        fi
        printf "present\n" > "${steam_cfg_state}"
        return 0
    fi

    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] printf 'absent\\n' > ${steam_cfg_state}"
        return 0
    fi

    printf "absent\n" > "${steam_cfg_state}"
}

configure_steam_cfg_inhibit_all() {
    local steam_cfg="${STEAM_DIR}/${STEAM_CFG_NAME}"

    backup_steam_cfg_state_if_needed
    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] append managed Steam.cfg marker + ${STEAM_CFG_INHIBIT_ALL_LINE} into ${steam_cfg}"
        return 0
    fi

    local stripped_cfg
    stripped_cfg=$(mktemp /tmp/millennium-steamcfg-stripped.XXXXXX)
    local merged_cfg
    merged_cfg=$(mktemp /tmp/millennium-steamcfg-merged.XXXXXX)
    strip_managed_steam_cfg_lines "${steam_cfg}" "${stripped_cfg}"

    : > "${merged_cfg}"
    if [ -s "${stripped_cfg}" ]; then
        cat "${stripped_cfg}" > "${merged_cfg}"
        printf "\n" >> "${merged_cfg}"
    fi

    printf "%s\n" "${STEAM_CFG_MANAGED_MARKER}" >> "${merged_cfg}"
    printf "%s\n" "${STEAM_CFG_INHIBIT_ALL_LINE}" >> "${merged_cfg}"

    mv -f "${merged_cfg}" "${steam_cfg}"
    rm -f "${stripped_cfg}" "${merged_cfg}"
    log "Configured ${STEAM_CFG_NAME} with ${STEAM_CFG_INHIBIT_ALL_LINE}."
}

restore_steam_cfg_state() {
    local steam_cfg="${STEAM_DIR}/${STEAM_CFG_NAME}"
    local steam_cfg_backup="${STEAM_DIR}/${STEAM_CFG_BACKUP_NAME}"
    local steam_cfg_state="${STEAM_DIR}/${STEAM_CFG_STATE_NAME}"

    if [ ! -f "${steam_cfg_state}" ]; then
        log "No managed Steam.cfg state found."
        return 0
    fi

    local state_value
    state_value=$(tr -d '\r\n' < "${steam_cfg_state}" || true)

    case "${state_value}" in
        present)
            if [ -f "${steam_cfg_backup}" ]; then
                run_cmd mv -f "${steam_cfg_backup}" "${steam_cfg}"
                log "Restored original ${STEAM_CFG_NAME} from backup."
            else
                log "Missing ${STEAM_CFG_BACKUP_NAME}; keeping current ${STEAM_CFG_NAME}."
            fi
            ;;
        absent)
            if [ -f "${steam_cfg}" ]; then
                if [ "${DRY_RUN}" -eq 1 ]; then
                    log "[DRY RUN] remove managed lines from ${steam_cfg}, and delete file if it becomes empty"
                    run_cmd rm -f "${steam_cfg_backup}"
                    run_cmd rm -f "${steam_cfg_state}"
                    return 0
                fi

                local stripped_cfg
                stripped_cfg=$(mktemp /tmp/millennium-steamcfg-restore.XXXXXX)

                strip_managed_steam_cfg_lines "${steam_cfg}" "${stripped_cfg}"

                if rg -q '[^[:space:]]' "${stripped_cfg}"; then
                    mv -f "${stripped_cfg}" "${steam_cfg}"
                    log "Removed managed Steam.cfg entries and kept remaining user entries."
                else
                    rm -f "${steam_cfg}" "${stripped_cfg}"
                    log "Removed managed ${STEAM_CFG_NAME}."
                fi
            fi
            run_cmd rm -f "${steam_cfg_backup}"
            ;;
        *)
            log "Unknown Steam.cfg managed state '${state_value}', keeping current ${STEAM_CFG_NAME}."
            run_cmd rm -f "${steam_cfg_backup}"
            ;;
    esac

    run_cmd rm -f "${steam_cfg_state}"
}

build_runtime_search_dirs() {
    RUNTIME_SEARCH_DIRS=()
    if [ -n "${LEGACY_BUILD_DIR}" ]; then
        RUNTIME_SEARCH_DIRS=("${LEGACY_BUILD_DIR}")
        return 0
    fi

    if [ -n "${RUNTIME_BUILD_DIR}" ]; then
        RUNTIME_SEARCH_DIRS=("${RUNTIME_BUILD_DIR}")
        return 0
    fi

    RUNTIME_SEARCH_DIRS=("${DEFAULT_RUNTIME_SEARCH_DIRS[@]}")
}

sync_runtime_artifacts() {
    local strict_mode="$1"
    build_runtime_search_dirs

    for artifact in "${REQUIRED_RUNTIME_ARTIFACTS[@]}"; do
        local source_path
        source_path=$(find_artifact_optional "${artifact}" "${RUNTIME_SEARCH_DIRS[@]}")

        if [ -n "${source_path}" ]; then
            install_file "${source_path}" "${STEAM_DIR}/${artifact}"
            continue
        fi

        if [ -f "${STEAM_DIR}/${artifact}" ]; then
            log "Keeping existing ${artifact} in Steam bundle."
            continue
        fi

        if [ "${strict_mode}" -eq 1 ]; then
            fail "Missing required build artifact '${artifact}'."
        fi

        fail "Missing ${artifact} in both build output and Steam bundle."
    done

    for artifact in "${OPTIONAL_RUNTIME_ARTIFACTS[@]}"; do
        local source_path=""
        source_path=$(find_artifact_optional "${artifact}" "${WRAPPER_BUILD_DIR}" "${RUNTIME_SEARCH_DIRS[@]}")

        if [ -n "${source_path}" ]; then
            install_file "${source_path}" "${STEAM_DIR}/${artifact}"
            continue
        fi

        if [ -f "${STEAM_DIR}/${artifact}" ]; then
            log "Keeping existing optional artifact ${artifact}."
        else
            log "Optional artifact ${artifact} is not present (this is allowed)."
        fi
    done
}

prepare_tier0_backup() {
    local current_tier0="${STEAM_DIR}/${TIER0_NAME}"
    local backup_tier0="${STEAM_DIR}/${TIER0_REAL_NAME}"

    if [ ! -f "${current_tier0}" ] && [ ! -f "${backup_tier0}" ]; then
        fail "Neither ${TIER0_NAME} nor ${TIER0_REAL_NAME} exists in ${STEAM_DIR}."
    fi

    if [ -f "${current_tier0}" ] && ! is_tier0_proxy "${current_tier0}"; then
        run_cmd mv -f "${current_tier0}" "${backup_tier0}"
    fi

    if [ "${DRY_RUN}" -eq 1 ]; then
        return 0
    fi

    if [ ! -f "${backup_tier0}" ]; then
        fail "Expected ${backup_tier0} to exist after backup step."
    fi
}

compile_tier0_proxy() {
    local output_path="$1"
    local backup_tier0="${STEAM_DIR}/${TIER0_REAL_NAME}"
    local backup_install_name="@loader_path/${TIER0_NAME}"

    [ -f "${PROXY_SOURCE}" ] || fail "Proxy source missing: ${PROXY_SOURCE}"
    if [ ! -f "${backup_tier0}" ] && [ "${DRY_RUN}" -eq 0 ]; then
        fail "Missing backup tier0 at ${backup_tier0}"
    fi

    if [ "${DRY_RUN}" -eq 0 ]; then
        local detected_install_name
        detected_install_name=$(otool -D "${backup_tier0}" 2>/dev/null | sed -n '2p' || true)
        if [ -n "${detected_install_name}" ]; then
            backup_install_name="${detected_install_name}"
        fi
    fi

    command -v clang++ >/dev/null 2>&1 || fail "clang++ is required to compile the tier0 proxy."

    run_cmd clang++ \
        -std=c++17 \
        -dynamiclib \
        -fvisibility=hidden \
        -arch arm64 \
        -arch x86_64 \
        -Wl,-install_name,@loader_path/${TIER0_NAME}.proxy \
        -Wl,-reexport_library,"${backup_tier0}" \
        "${PROXY_SOURCE}" \
        -o "${output_path}"

    run_cmd install_name_tool -change "${backup_install_name}" "@loader_path/${TIER0_REAL_NAME}" "${output_path}"
    run_cmd install_name_tool -id "@loader_path/${TIER0_NAME}" "${output_path}"

    if [ "${DRY_RUN}" -eq 0 ] && ! is_tier0_proxy "${output_path}"; then
        fail "Compiled proxy did not expose the expected marker symbol."
    fi
}

install_or_repair() {
    local strict_runtime="$1"

    ensure_steam_closed
    [ -d "${STEAM_DIR}" ] || fail "Steam runtime directory not found: ${STEAM_DIR}"

    sync_runtime_artifacts "${strict_runtime}"
    prepare_tier0_backup

    local temp_dir
    temp_dir=$(mktemp -d /tmp/millennium-tier0-proxy.XXXXXX)
    local proxy_output="${temp_dir}/${TIER0_NAME}"

    if [ "${DRY_RUN}" -eq 0 ]; then
        trap 'rm -rf "${temp_dir}"' RETURN
    fi

    compile_tier0_proxy "${proxy_output}"
    install_file "${proxy_output}" "${STEAM_DIR}/${TIER0_NAME}"

    if [ "${DRY_RUN}" -eq 0 ] && ! is_tier0_proxy "${STEAM_DIR}/${TIER0_NAME}"; then
        fail "Installed ${TIER0_NAME} is not the Millennium tier0 proxy."
    fi

    if [ "${CONFIGURE_STEAM_CFG_INHIBIT_ALL}" -eq 1 ]; then
        configure_steam_cfg_inhibit_all
    fi

    if [ "${CONFIGURE_ICON_REPLACEMENT}" -eq 1 ]; then
        install_custom_icon
    fi

    log "Tier0 proxy is active in ${STEAM_DIR}."
    log "If Steam updates overwrite it, run: $(basename "$0") --repair"
}

uninstall_runtime() {
    ensure_steam_closed

    local current_tier0="${STEAM_DIR}/${TIER0_NAME}"
    local backup_tier0="${STEAM_DIR}/${TIER0_REAL_NAME}"

    [ -d "${STEAM_DIR}" ] || fail "Steam runtime directory not found: ${STEAM_DIR}"
    [ -f "${backup_tier0}" ] || fail "No backup found at ${backup_tier0}"

    if [ -f "${current_tier0}" ]; then
        run_cmd rm -f "${current_tier0}"
    fi

    run_cmd mv "${backup_tier0}" "${current_tier0}"

    for artifact in "${REQUIRED_RUNTIME_ARTIFACTS[@]}" "${OPTIONAL_RUNTIME_ARTIFACTS[@]}"; do
        if [ -f "${STEAM_DIR}/${artifact}" ]; then
            run_cmd rm -f "${STEAM_DIR}/${artifact}"
        fi
    done

    restore_custom_icon
    restore_steam_cfg_state

    log "Restored original ${TIER0_NAME} and removed Millennium runtime artifacts."
}

status_runtime() {
    local current_tier0="${STEAM_DIR}/${TIER0_NAME}"
    local backup_tier0="${STEAM_DIR}/${TIER0_REAL_NAME}"
    local cef_marker="${STEAM_DIR}/${CEF_MARKER_NAME}"
    local bundle_root=""
    bundle_root=$(resolve_steam_bundle_root 2>/dev/null || true)
    local bundle_icon_override_path=""
    local bundle_icon_override_backup=""
    local bundle_finderinfo_backup=""
    if [ -n "${bundle_root}" ]; then
        bundle_icon_override_path="${bundle_root}/"$'Icon\r'
        bundle_icon_override_backup="${bundle_root}/${BUNDLE_ICON_OVERRIDE_BACKUP_NAME}"
        bundle_finderinfo_backup="${bundle_root}/${BUNDLE_FINDERINFO_BACKUP_NAME}"
    fi
    local icon_targets=()
    while IFS= read -r icon_target; do
        [ -n "${icon_target}" ] || continue
        icon_targets+=("${icon_target}")
    done < <(resolve_steam_icon_paths 2>/dev/null || true)
    local steam_cfg="${STEAM_DIR}/${STEAM_CFG_NAME}"
    local steam_cfg_backup="${STEAM_DIR}/${STEAM_CFG_BACKUP_NAME}"
    local steam_cfg_state="${STEAM_DIR}/${STEAM_CFG_STATE_NAME}"

    log "Steam directory: ${STEAM_DIR}"

    if [ -f "${current_tier0}" ]; then
        if is_tier0_proxy "${current_tier0}"; then
            log "tier0 proxy: installed"
        else
            log "tier0 proxy: not installed"
        fi
    else
        log "tier0 proxy: missing current ${TIER0_NAME}"
    fi

    if [ -f "${backup_tier0}" ]; then
        log "tier0 backup: present (${TIER0_REAL_NAME})"
    else
        log "tier0 backup: missing"
    fi

    for artifact in "${REQUIRED_RUNTIME_ARTIFACTS[@]}" "${OPTIONAL_RUNTIME_ARTIFACTS[@]}"; do
        if [ -f "${STEAM_DIR}/${artifact}" ]; then
            log "${artifact}: present"
        else
            log "${artifact}: missing"
        fi
    done

    if [ -f "${cef_marker}" ]; then
        log "${CEF_MARKER_NAME}: present"
    else
        log "${CEF_MARKER_NAME}: missing (the tier0 proxy recreates it on Steam launch)"
    fi

    if [ "${#icon_targets[@]}" -eq 0 ]; then
        log "Steam icon: failed to resolve target path"
    else
        for icon_target in "${icon_targets[@]}"; do
            local icon_backup="${icon_target}${ICON_BACKUP_SUFFIX}"
            if [ -f "${icon_target}" ]; then
                log "Steam icon target: present (${icon_target})"
            else
                log "Steam icon target: missing (${icon_target})"
            fi

            if [ -f "${icon_backup}" ]; then
                log "Steam icon backup: present (${icon_backup})"
            else
                log "Steam icon backup: missing (${icon_backup})"
            fi
        done
    fi

    if [ -n "${bundle_root}" ]; then
        if [ -f "${bundle_icon_override_path}" ]; then
            log "Bundle icon override file: present (Icon\\r)"
        else
            log "Bundle icon override file: missing (Icon\\r)"
        fi

        if xattr -p com.apple.FinderInfo "${bundle_root}" >/dev/null 2>&1; then
            log "Bundle FinderInfo xattr: present"
        else
            log "Bundle FinderInfo xattr: missing"
        fi

        if [ -f "${bundle_icon_override_backup}" ]; then
            log "Bundle icon override backup: present (${bundle_icon_override_backup})"
        else
            log "Bundle icon override backup: missing (${bundle_icon_override_backup})"
        fi

        if [ -f "${bundle_finderinfo_backup}" ]; then
            log "Bundle FinderInfo backup: present (${bundle_finderinfo_backup})"
        else
            log "Bundle FinderInfo backup: missing (${bundle_finderinfo_backup})"
        fi
    else
        log "Bundle icon override status: failed to resolve Steam bundle root"
    fi

    if [ -f "${ICON_SOURCE_PATH}" ]; then
        log "Custom icon source: present (${ICON_SOURCE_PATH})"
    else
        log "Custom icon source: missing (${ICON_SOURCE_PATH})"
    fi

    if [ -f "${steam_cfg}" ]; then
        if rg -q '^[[:space:]]*BootStrapperInhibitAll[[:space:]]*=[[:space:]]*enable[[:space:]]*$' "${steam_cfg}"; then
            log "${STEAM_CFG_NAME}: BootStrapperInhibitAll=enable"
        else
            log "${STEAM_CFG_NAME}: present (no BootStrapperInhibitAll=enable)"
        fi
    else
        log "${STEAM_CFG_NAME}: missing"
    fi

    if [ -f "${steam_cfg_state}" ]; then
        log "${STEAM_CFG_STATE_NAME}: present (managed restore state recorded)"
    else
        log "${STEAM_CFG_STATE_NAME}: missing"
    fi

    if [ -f "${steam_cfg_backup}" ]; then
        log "${STEAM_CFG_BACKUP_NAME}: present"
    else
        log "${STEAM_CFG_BACKUP_NAME}: missing"
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ $# -ge 2 ] || fail "Missing value for --build-dir"
            LEGACY_BUILD_DIR="$2"
            RUNTIME_BUILD_DIR="$2"
            WRAPPER_BUILD_DIR="$2"
            shift 2
            ;;
        --runtime-build-dir)
            [ $# -ge 2 ] || fail "Missing value for --runtime-build-dir"
            RUNTIME_BUILD_DIR="$2"
            shift 2
            ;;
        --wrapper-build-dir)
            [ $# -ge 2 ] || fail "Missing value for --wrapper-build-dir"
            WRAPPER_BUILD_DIR="$2"
            shift 2
            ;;
        --steam-dir)
            [ $# -ge 2 ] || fail "Missing value for --steam-dir"
            STEAM_DIR="$2"
            shift 2
            ;;
        --repair)
            MODE="repair"
            shift
            ;;
        --restore-steam-cfg)
            MODE="restore-steam-cfg"
            shift
            ;;
        --status)
            MODE="status"
            shift
            ;;
        --uninstall)
            MODE="uninstall"
            shift
            ;;
        --steam-cfg-inhibit-all)
            CONFIGURE_STEAM_CFG_INHIBIT_ALL=1
            shift
            ;;
        --replace-icon)
            CONFIGURE_ICON_REPLACEMENT=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --restore-icon)
            MODE="restore-icon"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "Unknown argument: $1"
            ;;
    esac
done

case "${MODE}" in
    install)
        install_or_repair 1
        ;;
    repair)
        install_or_repair 0
        ;;
    uninstall)
        uninstall_runtime
        ;;
    restore-steam-cfg)
        ensure_steam_closed
        [ -d "${STEAM_DIR}" ] || fail "Steam runtime directory not found: ${STEAM_DIR}"
        restore_steam_cfg_state
        ;;
    restore-icon)
        ensure_steam_closed
        [ -d "${STEAM_DIR}" ] || fail "Steam runtime directory not found: ${STEAM_DIR}"
        restore_custom_icon
        ;;
    status)
        status_runtime
        ;;
    *)
        fail "Unsupported mode: ${MODE}"
        ;;
esac
