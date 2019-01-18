#!/bin/bash

waf_config_cmd="./waf configure --prefix=/usr"
waf_cmd="./waf"

# If we are debugging, do some prep and set the cmake flags
if [[ $* == *--debug* ]]; then
    waf_config_cmd="$waf_config_cmd --debug"
    waf_cmd="$waf_cmd --debug"
    if [ ! -e /DEBUG_NDN_ICP ]; then
#        sed -i 's/cxxflags=\[/cxxflags=\["-ggdb", "-O0"/g' /code/ndn-icp-download/wscript
        touch /DEBUG_NDN_ICP
    fi
# If using valgrind
elif [[ $* == *--valgrind* ]]; then
    if [ ! -e /DEBUG_NDN_DASH ]; then
        echo "no debug ndn dash"
        eval "/rebuild-player.sh --debug"
    fi
    if [ ! -e /DEBUG_NDN_ICP ]; then
        eval "/rebuild-ndn-icp.sh --debug"
    fi
    exec_cmd="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=/
..valgrind-out.txt $exec_cmd"
# If we aren't debugging, revert the debug prep
else
    if [ -e /DEBUG_NDN_ICP ]; then
#        sed -i 's/cxxflags=\["-ggdb", "-O0"/cxxflags=\[/g' /code/ndn-icp-download/wscript
        rm /DEBUG_NDN_ICP
    fi
fi

# Execute
cd /code/ndn-icp-download
./waf clean distclean
eval $waf_config_cmd
eval $waf_cmd
./waf install
