#!/bin/sh

# This function filters out the error message "from LD_PRELOAD cannot be preloaded" from 64 bit executables 
# The messages are just failing module side effects and are not fatal. 
filter_output() {
    local patterns=('from LD_PRELOAD cannot be preloaded' 'Fontconfig warning: "' 'Fontconfig error: "')
    while IFS= read -r msg; do
        local skip=false
        for pattern in "${patterns[@]}"; do
            if [[ "$msg" =~ $pattern ]]; then
                skip=true
                break
            fi
        done
        if [[ "$skip" == false ]]; then
            printf '%s\n' "$msg"
        fi
    done
}

steam_output() {
    while IFS= read -r msg; do
        printf '%s\n' "$msg"
    done
}

# If there is no display, keep original settings and redirect stdout back to its original file descriptor
if [ -n "$DISPLAY" ]; then
    exec 3>&1 # Save a copy of file descriptor 1 (stdout) so we can restore it later
    exec 1> >(filter_output) # Redirect stdout to filter_output
    export STEAM_RUNTIME_LOGGER=0 # On archlinux, this needed to stop stdout from being piped into /dev/null instead of the terminal
fi

export LD_PRELOAD="$HOME/.millennium/libMillennium.so${LD_PRELOAD:+:$LD_PRELOAD}" # preload Millennium into Steam
export LD_LIBRARY_PATH="$HOME/.millennium/${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Millennium hooks __libc_start_main to initialize itself, which is a function that is called before main. 
# Besides that, Millennium does not alter Steam memory and runs completely disjoint.
exec /usr/lib/steam/steam > >(steam_output) 2>&1 "$@"
