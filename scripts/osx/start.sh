#!/bin/bash

# This function filters out the error message "from LD_PRELOAD cannot be preloaded" from 64 bit executables 
# The messages are just failing module side effects and not fatal. 
filter_output() {
    printf '%s\n' "$msg"
}

steam_output() {
    while IFS= read -r msg; do
        printf '\033[35mSTEAM\033[0m %s\n' "$msg"
    done
}

# Save a copy of file descriptor 1 (stdout) so we can restore it later
exec 3>&1
exec 1> >(filter_output) # Redirect stdout to filter_output

export STEAM_RUNTIME_LOGGER=0 # On archlinux, this needed to stop stdout from being piped into /dev/null instead of the terminal
export LD_PRELOAD="~/.millennium/libMillennium.dylib${LD_PRELOAD:+:$LD_PRELOAD}" # preload Millennium into Steam
export LD_LIBRARY_PATH="~/.millennium/${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Start Steam and get its PID
/Applications/Steam.app/Contents/MacOS/steam_osx > >(steam_output) 2>&1 &
STEAM_PID=$!

# Wait for Steam to exit
while ps -p $STEAM_PID > /dev/null; do
    sleep 1
done

echo "Steam has exited. Closing script."
