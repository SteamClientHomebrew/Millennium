#!/bin/sh

export LD_LIBRARY_PATH=$(pwd):$LD_LIBRARY_PATH
STEAM_EXECUTABLE="/usr/bin/steam"

start_user32() {
    echo "Starting user32..."
    ./user32 &
    USER32_PID=$!
}

while true; do
    if [ -z "$(pgrep -x steam)" ]; then
        echo "Waiting for Steam to start..."
        inotifywait -e open "$STEAM_EXECUTABLE"
        echo "Steam is now open!"
    fi

    start_user32

    echo "Waiting for Steam to close..."
    
    while [ -n "$(pgrep -x steam)" ]; do
        inotifywait -e close "$STEAM_EXECUTABLE" >/dev/null 2>&1
    done

    echo "Steam has closed. Killing user32..."
    if [ -n "$USER32_PID" ]; then
        kill $USER32_PID
        wait $USER32_PID 2>/dev/null
    fi
done
