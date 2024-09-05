#!/bin/bash

# This function filters out the error message "from LD_PRELOAD cannot be preloaded" from 64 bit executables 
# The messages are just failing module side effects and not fatal. 
filter_output() {
    local pattern='from LD_PRELOAD cannot be preloaded'
    while IFS= read -r msg; do
        if [[ ! "$msg" =~ $pattern ]]; then
            printf '%s\n' "$msg"
        fi
    done
}

steam_output() {
    while IFS= read -r msg; do
        printf '\033[35mSTEAM\033[0m %s\n' "$msg"
    done
}

exec 3>&1 # Save a copy of file descriptor 1 (stdout) so we can restore it later
exec 1> >(filter_output) # Redirect stdout to filter_output

export STEAM_RUNTIME_LOGGER=0 # On archlinux, this needed to stop stdout from being piped into /dev/null instead of the terminal
export LD_PRELOAD="$HOME/.millennium/libMillennium.so${LD_PRELOAD:+:$LD_PRELOAD}" # preload Millennium into Steam
export LD_LIBRARY_PATH="$HOME/.millennium/${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Millennium hooks __libc_start_main to initialize itself, which is a function that is called before main. 
# Besides that, Millennium does not alter Steam memory and runs completely disjoint.

bash ~/.steam/steam/steam.sh > >(steam_output) 2>&1 "$@"
