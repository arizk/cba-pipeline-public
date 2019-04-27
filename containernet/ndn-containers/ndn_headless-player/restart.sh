#!/bin/bash

exec_cmd="/code/ndn-dash/libdash/qtsampleplayer/build/qtsampleplayer -nohead -n 0.8 -bola 44 -u /n/mpd"

# Stop the current process
pkill -9 qtsampleplayer

# Remove previous logs
rm /log
rm /player-out.txt

# If we are debugging, do some prep and set the cmake flags
if [[ $* == *--debug* ]]; then
    if [ ! -e /DEBUG_NDN_DASH ]; then
        eval "/rebuild-player.sh --debug"
    fi
    if [ ! -e /DEBUG_NDN_ICP ]; then
        eval "/rebuild-ndn-icp.sh --debug"
    fi
    exec_cmd="gdb --args $exec_cmd"
# If we aren't debugging, revert the debug prep
else
    if [ -e /DEBUG_NDN_DASH ]; then
        eval "/rebuild-player.sh"
    fi
    if [ -e /DEBUG_NDN_ICP ]; then
        eval "/rebuild-ndn-icp.sh"
    fi
fi

# If we're rerouting our output, be sure we can
if [[ $* == *--player-out* ]]; then
    if [[ $* == *--debug* ]]; then
        echo "Error; cannot debug and reroute output"
        exit 1
    fi
    exec_cmd="$exec_cmd >> player-out.txt &"
fi

# Execute
eval $exec_cmd
