#!/bin/bash
SERVER="NEATVI"

# Check if server is running, if not start it
if ! ./vi --serverlist | grep -q "^${SERVER}$"; then
    ./vi --servername "$SERVER" "$1" &
else
    ./vi --remote "$1" --servername "$SERVER"
fi
