#!/usr/bin/env bash

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly PROXY_SOURCE="${REPO_ROOT}/src/bootstrap/macos/tier0_proxy.cc"
readonly DEFAULT_STEAM_DIR="${HOME}/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS"
readonly DEFAULT_WRAPPER_BUILD_DIR="${REPO_ROOT}/build/osx-debug/src/bootstrap/macos"
readonly DEFAULT_RUNTIME_SEARCH_DIRS=(
    "${REPO_ROOT}/build/osx-debug/src"
    "${REPO_ROOT}/build/osx-release/src"
    "${REPO_ROOT}/build/src"
    "${REPO_ROOT}/build"
)
readonly REQUIRED_RUNTIME_ARTIFACTS=(
    "libmillennium.dylib"
    "libmillennium_hhx64.dylib"
)
readonly OPTIONAL_RUNTIME_ARTIFACTS=(
    "libmillennium_child_hook.dylib"
)
readonly WRAPPER_APP_NAME="Steam Millennium.app"
readonly WRAPPER_EXECUTABLE_NAME="steam_osx"
readonly WRAPPER_BOOTSTRAP_ARTIFACT="libmillennium_bootstrap.dylib"
readonly WRAPPER_RUNTIME_ARTIFACTS=(
    "libmillennium.dylib"
    "libmillennium_hhx64.dylib"
    "libmillennium_child_hook.dylib"
)
readonly WRAPPER_ASSET_RELATIVE_DIR="millennium-assets"
readonly APP_INFO_TEMPLATE="${REPO_ROOT}/src/bootstrap/macos/Info.plist.xml"
readonly APP_ICON_SOURCE="${REPO_ROOT}/src/bootstrap/macos/AppIcon.icns"
readonly DEFAULT_APP_BUNDLE_GLOBAL="/Applications/${WRAPPER_APP_NAME}"
readonly DEFAULT_APP_BUNDLE_USER="${HOME}/Applications/${WRAPPER_APP_NAME}"
readonly DEFAULT_RUNTIME_INSTALL_ROOT="${HOME}/Library/Application Support/Millennium/runtime"

readonly TIER0_NAME="libtier0_s.dylib"
readonly TIER0_REAL_NAME="libtier0_s.real.dylib"
readonly CEF_MARKER_NAME=".cef-enable-remote-debugging"
readonly STEAM_CFG_NAME="Steam.cfg"
readonly STEAM_CFG_BACKUP_NAME="Steam.cfg.millennium.backup"
readonly STEAM_CFG_STATE_NAME="Steam.cfg.millennium.state"
readonly STEAM_CFG_MANAGED_MARKER="# Added by Millennium install_macos.sh"
readonly STEAM_CFG_INHIBIT_ALL_LINE="BootStrapperInhibitAll=enable"

MODE="install"
LEGACY_BUILD_DIR=""
RUNTIME_BUILD_DIR=""
WRAPPER_BUILD_DIR="${DEFAULT_WRAPPER_BUILD_DIR}"
STEAM_DIR="${DEFAULT_STEAM_DIR}"
APP_BUNDLE_PATH=""
RUNTIME_INSTALL_ROOT="${DEFAULT_RUNTIME_INSTALL_ROOT}"
DRY_RUN=0
CONFIGURE_STEAM_CFG_INHIBIT_ALL=0
RUNTIME_SEARCH_DIRS=()
SKIP_STEAM_CFG_INHIBIT_ALL=0
INSTALL_VARIANT="wrapper-app"

log() { printf "%b\n" "$1"; }

fail() {
    log "$1" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [--build-dir <dir>] [--runtime-build-dir <dir>] [--wrapper-build-dir <dir>] [--steam-dir <dir>] [--app-bundle <path>] [--runtime-install-root <path>] [--dry-run] [--repair] [--status] [--uninstall] [--wrapper-app] [--tier0-legacy] [--steam-cfg-inhibit-all] [--skip-steam-cfg-inhibit-all] [--restore-steam-cfg]

Default mode installs a wrapper app (${WRAPPER_APP_NAME}) without modifying Steam tracked files:
1. Installs launcher/runtime artifacts into ${DEFAULT_RUNTIME_INSTALL_ROOT}
2. Builds/updates ${WRAPPER_APP_NAME} at ${DEFAULT_APP_BUNDLE_GLOBAL}
3. Launch Millennium by opening that app bundle

Modes:
  (default)   install           Install/update wrapper app + runtime payload
  --repair                      Re-install wrapper app/runtime payload
  --status                      Print current install state
  --uninstall                   Remove wrapper app/runtime payload
  --wrapper-app                 Force wrapper-app install flow (default)
  --tier0-legacy                Use legacy tier0 proxy install flow (modifies Steam bundle)
  --restore-steam-cfg           Tier0-only: restore Steam.cfg to pre-managed state

Notes:
- Wrapper-app mode keeps Steam updates enabled; users should launch via ${WRAPPER_APP_NAME}.
- Tier0 mode is legacy and can be overwritten by Steam updates.
- Tier0 mode defaults to writing ${STEAM_CFG_INHIBIT_ALL_LINE}; use --skip-steam-cfg-inhibit-all to skip it.
EOF
}

run_cmd() {
    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] $*"
        return 0
    fi

    "$@"
}

resolve_preferred_app_bundle_path() {
    if [ -e "${DEFAULT_APP_BUNDLE_GLOBAL}" ]; then
        printf "%s\n" "${DEFAULT_APP_BUNDLE_GLOBAL}"
        return 0
    fi

    if [ -e "${DEFAULT_APP_BUNDLE_USER}" ]; then
        printf "%s\n" "${DEFAULT_APP_BUNDLE_USER}"
        return 0
    fi

    printf "%s\n" "${DEFAULT_APP_BUNDLE_GLOBAL}"
    return 0
}

ensure_steam_closed() {
    if pgrep -x "steam_osx" >/dev/null 2>&1 || pgrep -f "/Steam Helper" >/dev/null 2>&1; then
        fail "Quit Steam before modifying the runtime bundle."
    fi
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

install_data_file() {
    local source_path="$1"
    local destination_path="$2"

    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] install -m 0644 ${source_path} ${destination_path}"
        return 0
    fi

    install -m 0644 "${source_path}" "${destination_path}"
}

ensure_directory() {
    local directory="$1"
    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] mkdir -p ${directory}"
        return 0
    fi

    mkdir -p "${directory}"
}

install_symlink() {
    local target_path="$1"
    local link_path="$2"

    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] ln -sfn ${target_path} ${link_path}"
        return 0
    fi

    ln -sfn "${target_path}" "${link_path}"
}

remove_path_recursive() {
    local target_path="$1"
    if [ "${DRY_RUN}" -eq 1 ]; then
        log "[DRY RUN] rm -rf ${target_path}"
        return 0
    fi

    rm -rf "${target_path}"
}

resolve_install_version() {
    local version
    version=$(git -C "${REPO_ROOT}" describe --tags --always --dirty 2>/dev/null || true)
    if [ -z "${version}" ]; then
        version=$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || true)
    fi
    if [ -z "${version}" ]; then
        version=$(date +%Y%m%d%H%M%S)
    fi

    version="${version//\//-}"
    version="${version// /-}"
    printf "%s\n" "${version}"
}

copy_wrapper_assets_into_runtime() {
    local runtime_dir="$1"
    local asset_root="${runtime_dir}/${WRAPPER_ASSET_RELATIVE_DIR}"
    local loader_target_dir="${asset_root}/loader"
    local chunks_target_dir="${loader_target_dir}/chunks"

    local loader_entry="${REPO_ROOT}/src/typescript/sdk/packages/loader/build/millennium.js"
    local chunks_source_dir="${REPO_ROOT}/src/typescript/sdk/packages/loader/build/chunks"
    local frontend_source="${REPO_ROOT}/build/frontend.bin"

    [ -f "${loader_entry}" ] || fail "Missing loader entry asset: ${loader_entry}"
    [ -d "${chunks_source_dir}" ] || fail "Missing loader chunks directory: ${chunks_source_dir}"
    [ -f "${frontend_source}" ] || fail "Missing frontend asset: ${frontend_source}"

    ensure_directory "${loader_target_dir}"
    ensure_directory "${chunks_target_dir}"

    install_data_file "${loader_entry}" "${loader_target_dir}/millennium.js"
    install_data_file "${frontend_source}" "${asset_root}/millennium-frontend.js"

    local found_chunk=0
    local chunk_file=""
    for chunk_file in "${chunks_source_dir}"/chunk-*.js; do
        [ -e "${chunk_file}" ] || continue
        found_chunk=1
        install_data_file "${chunk_file}" "${chunks_target_dir}/$(basename "${chunk_file}")"
    done

    if [ "${found_chunk}" -eq 0 ]; then
        fail "No chunk-*.js files found in ${chunks_source_dir}"
    fi
}

resolve_wrapper_artifact() {
    local artifact_name="$1"
    local source_path=""
    source_path=$(find_artifact_optional "${artifact_name}" "${WRAPPER_BUILD_DIR}" "${RUNTIME_SEARCH_DIRS[@]}")
    if [ -z "${source_path}" ]; then
        fail "Missing build artifact '${artifact_name}'."
    fi
    printf "%s\n" "${source_path}"
}

install_wrapper_runtime_payload() {
    local runtime_dir="$1"

    build_runtime_search_dirs
    ensure_directory "${runtime_dir}"

    local wrapper_executable=""
    wrapper_executable=$(resolve_wrapper_artifact "${WRAPPER_EXECUTABLE_NAME}")
    install_file "${wrapper_executable}" "${runtime_dir}/${WRAPPER_EXECUTABLE_NAME}"

    local bootstrap_artifact=""
    bootstrap_artifact=$(resolve_wrapper_artifact "${WRAPPER_BOOTSTRAP_ARTIFACT}")
    install_file "${bootstrap_artifact}" "${runtime_dir}/${WRAPPER_BOOTSTRAP_ARTIFACT}"

    local artifact=""
    for artifact in "${WRAPPER_RUNTIME_ARTIFACTS[@]}"; do
        local runtime_artifact=""
        runtime_artifact=$(resolve_wrapper_artifact "${artifact}")
        install_file "${runtime_artifact}" "${runtime_dir}/${artifact}"
    done

    copy_wrapper_assets_into_runtime "${runtime_dir}"
}

install_wrapper_app_bundle() {
    local runtime_dir="$1"
    local app_contents_dir="${APP_BUNDLE_PATH}/Contents"
    local app_macos_dir="${app_contents_dir}/MacOS"
    local app_resources_dir="${app_contents_dir}/Resources"

    [ -f "${APP_INFO_TEMPLATE}" ] || fail "Missing app Info.plist template: ${APP_INFO_TEMPLATE}"
    [ -f "${APP_ICON_SOURCE}" ] || fail "Missing app icon: ${APP_ICON_SOURCE}"

    ensure_directory "${app_macos_dir}"
    ensure_directory "${app_resources_dir}"

    install_data_file "${APP_INFO_TEMPLATE}" "${app_contents_dir}/Info.plist"
    install_data_file "${APP_ICON_SOURCE}" "${app_resources_dir}/AppIcon.icns"

    install_symlink "${runtime_dir}/${WRAPPER_EXECUTABLE_NAME}" "${app_macos_dir}/${WRAPPER_EXECUTABLE_NAME}"
    install_symlink "${runtime_dir}/${WRAPPER_BOOTSTRAP_ARTIFACT}" "${app_macos_dir}/${WRAPPER_BOOTSTRAP_ARTIFACT}"

    local artifact=""
    for artifact in "${WRAPPER_RUNTIME_ARTIFACTS[@]}"; do
        install_symlink "${runtime_dir}/${artifact}" "${app_macos_dir}/${artifact}"
    done

    install_symlink "${runtime_dir}/${WRAPPER_ASSET_RELATIVE_DIR}" "${app_resources_dir}/${WRAPPER_ASSET_RELATIVE_DIR}"
}

install_or_repair_wrapper_app() {
    ensure_steam_closed

    if [ "${APP_BUNDLE_PATH}" = "${DEFAULT_APP_BUNDLE_GLOBAL}" ] && [ ! -w "/Applications" ] && [ ! -e "${DEFAULT_APP_BUNDLE_GLOBAL}" ]; then
        APP_BUNDLE_PATH="${DEFAULT_APP_BUNDLE_USER}"
        log "No write access to /Applications, falling back to ${APP_BUNDLE_PATH}."
    fi

    local version=""
    version=$(resolve_install_version)
    local runtime_dir="${RUNTIME_INSTALL_ROOT}/${version}"
    local runtime_current="${RUNTIME_INSTALL_ROOT}/current"

    install_wrapper_runtime_payload "${runtime_dir}"
    install_symlink "${runtime_dir}" "${runtime_current}"
    install_wrapper_app_bundle "${runtime_current}"

    log "Installed wrapper app: ${APP_BUNDLE_PATH}"
    log "Launch Steam via: open '${APP_BUNDLE_PATH}'"
    log "If Steam is already running, fully quit Steam first, then relaunch via the wrapper app."
}

status_wrapper_app() {
    local app_contents_dir="${APP_BUNDLE_PATH}/Contents"
    local app_macos_dir="${app_contents_dir}/MacOS"
    local runtime_current="${RUNTIME_INSTALL_ROOT}/current"

    log "Install variant: wrapper-app"
    log "App bundle: ${APP_BUNDLE_PATH}"
    if [ -d "${APP_BUNDLE_PATH}" ]; then
        log "app bundle status: present"
    else
        log "app bundle status: missing"
    fi

    if [ -L "${runtime_current}" ] || [ -d "${runtime_current}" ]; then
        log "runtime current: $(readlink "${runtime_current}" 2>/dev/null || echo "${runtime_current}")"
    else
        log "runtime current: missing"
    fi

    if [ -e "${app_macos_dir}/${WRAPPER_EXECUTABLE_NAME}" ]; then
        log "${WRAPPER_EXECUTABLE_NAME}: present"
    else
        log "${WRAPPER_EXECUTABLE_NAME}: missing"
    fi

    local artifact=""
    for artifact in "${WRAPPER_RUNTIME_ARTIFACTS[@]}" "${WRAPPER_BOOTSTRAP_ARTIFACT}"; do
        if [ -e "${app_macos_dir}/${artifact}" ]; then
            log "${artifact}: present"
        else
            log "${artifact}: missing"
        fi
    done
}

uninstall_wrapper_app() {
    ensure_steam_closed
    remove_path_recursive "${APP_BUNDLE_PATH}"
    remove_path_recursive "${RUNTIME_INSTALL_ROOT}"
    log "Removed wrapper app and runtime payloads."
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

    if [ "${SKIP_STEAM_CFG_INHIBIT_ALL}" -eq 0 ] || [ "${CONFIGURE_STEAM_CFG_INHIBIT_ALL}" -eq 1 ]; then
        configure_steam_cfg_inhibit_all
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

    restore_steam_cfg_state

    log "Restored original ${TIER0_NAME} and removed Millennium runtime artifacts."
}

status_runtime() {
    local current_tier0="${STEAM_DIR}/${TIER0_NAME}"
    local backup_tier0="${STEAM_DIR}/${TIER0_REAL_NAME}"
    local cef_marker="${STEAM_DIR}/${CEF_MARKER_NAME}"
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
        --app-bundle)
            [ $# -ge 2 ] || fail "Missing value for --app-bundle"
            APP_BUNDLE_PATH="$2"
            shift 2
            ;;
        --runtime-install-root)
            [ $# -ge 2 ] || fail "Missing value for --runtime-install-root"
            RUNTIME_INSTALL_ROOT="$2"
            shift 2
            ;;
        --steam-dir)
            [ $# -ge 2 ] || fail "Missing value for --steam-dir"
            STEAM_DIR="$2"
            shift 2
            ;;
        --tier0-legacy)
            INSTALL_VARIANT="tier0-legacy"
            shift
            ;;
        --wrapper-app)
            INSTALL_VARIANT="wrapper-app"
            shift
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
            SKIP_STEAM_CFG_INHIBIT_ALL=0
            shift
            ;;
        --skip-steam-cfg-inhibit-all)
            SKIP_STEAM_CFG_INHIBIT_ALL=1
            CONFIGURE_STEAM_CFG_INHIBIT_ALL=0
            shift
            ;;
        --dry-run)
            DRY_RUN=1
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

if [ -z "${APP_BUNDLE_PATH}" ]; then
    APP_BUNDLE_PATH=$(resolve_preferred_app_bundle_path)
fi

if [ "${INSTALL_VARIANT}" = "wrapper-app" ]; then
    if [ "${CONFIGURE_STEAM_CFG_INHIBIT_ALL}" -eq 1 ] || [ "${SKIP_STEAM_CFG_INHIBIT_ALL}" -eq 1 ]; then
        fail "--steam-cfg-inhibit-all and --skip-steam-cfg-inhibit-all are only valid with --tier0-legacy."
    fi

    case "${MODE}" in
        install|repair)
            install_or_repair_wrapper_app
            ;;
        uninstall)
            uninstall_wrapper_app
            ;;
        status)
            status_wrapper_app
            ;;
        restore-steam-cfg)
            fail "--restore-steam-cfg is only valid with --tier0-legacy."
            ;;
        *)
            fail "Unsupported mode: ${MODE}"
            ;;
    esac
    exit 0
fi

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
    status)
        status_runtime
        ;;
    *)
        fail "Unsupported mode: ${MODE}"
        ;;
esac
