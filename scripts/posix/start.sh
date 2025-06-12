#!/bin/bash

if grep -qi 'arch' /etc/os-release || command -v pacman &> /dev/null; then
    STEAM_PATH="/usr/lib/steam/steam"
else
    STEAM_PATH="/usr/lib/steam/bin_steam.sh"
fi

export OPENSSL_CONF=/dev/null
export LD_PRELOAD="/usr/lib/millennium/libmillennium_x86.so${LD_PRELOAD:+:$LD_PRELOAD}" # preload Millennium into Steam
export LD_LIBRARY_PATH="/usr/lib/millennium/${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Millennium hooks __libc_start_main to initialize itself, which is a function that is called before main. 
# Besides that, Millennium does not alter Steam memory and runs completely disjoint.

# If there is no display, keep original settings and redirect stdout back to its original file descriptor
if [ -z "$DISPLAY" ]; then
    exec $STEAM_PATH "$@"
fi

export STEAM_RUNTIME_LOGGER=0 # On archlinux, this needed to stop stdout from being piped into /dev/null instead of the terminal

if [[ -z "${DEBUGGER-}" ]]; then
    echo "Redirecting Steam output..."
    # The grep filters out the error message "from LD_PRELOAD cannot be preloaded" from 64 bit executables 
    # The messages are just failing module side effects and are not fatal. 
    exec $STEAM_PATH "$@" 2> >(grep -v -e'from LD_PRELOAD cannot be preloaded' -e'Fontconfig \(warning\|error\): "' 2>/dev/null)
else
    exec $STEAM_PATH "$@"
fi
