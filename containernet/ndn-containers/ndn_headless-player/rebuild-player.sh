#!/bin/bash

cmake_flags="-DLIBAV_ROOT_DIR=/usr/lib"

# If we are debugging, do some prep and set the cmake flags
if [[ $* == *--debug* ]]; then
    if [ ! -e /DEBUG_NDN_DASH ]; then
        sed -i 's/O3/O0/g' /code/ndn-dash/libdash/qtsampleplayer/CMakeLists.txt
        touch /DEBUG_NDN_DASH
    fi
    cmake_flags="$cmake_flags -DCMAKE_BUILD_TYPE=Debug"
# If we aren't debugging, revert the debug prep
else
    if [ -e /DEBUG_NDN_DASH ]; then
        sed -i 's/O0/O3/g' /code/ndn-dash/libdash/qtsampleplayer/CMakeLists.txt
        rm /DEBUG_NDN_DASH
    fi
fi

# Execute
cd /code/ndn-dash/libdash/qtsampleplayer/build
cmake .. $cmake_flags
make
