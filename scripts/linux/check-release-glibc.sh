#!/usr/bin/env bash
set -euo pipefail

artifact_dir=${1:?Usage: check-release-glibc.sh ARTIFACT_DIR}
max_glibc=2.39
failed=0

for artifact in "$artifact_dir"/*; do
    versions=$(LC_ALL=C readelf --version-info --wide "$artifact")
    requirements=$(awk '
        /^Version / { in_needs = /^Version needs section/ }
        in_needs {
            for (i = 1; i < NF; i++) {
                if ($i == "Name:" && $(i + 1) ~ /^GLIBC_/) print $(i + 1)
            }
        }
    ' <<< "$versions")

    for requirement in $requirements; do
        version=${requirement#GLIBC_}
        if [[ ! $version =~ ^[0-9]+(\.[0-9]+)+$ ]] || [[ $(printf '%s\n' "$version" "$max_glibc" | sort -V | tail -n 1) != "$max_glibc" ]]; then
            printf '%s requires %s, outside the glibc %s release baseline\n' "$artifact" "$requirement" "$max_glibc" >&2
            failed=1
        fi
    done
done

exit "$failed"
