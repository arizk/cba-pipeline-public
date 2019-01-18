#!/bin/sh
#sleep 5
export MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

setcap cap_net_raw=eip /usr/local/bin/nfd
setcap cap_net_raw,cap_net_admin=eip /usr/local/bin/nfd
#service nfd stop
#service nfd start

#Todo register route to server
#nfdc create ether://[00:16:3e:00:00:07]/server
#nfdc register /n $(nfd-status | grep 00:16:3e:00:00:07 | cut -d= -f2 | awk '{print $1}')

mkdir /root/log
#/bin/bash -c "nohup ifstat -i server -b -t > /root/log/link_client_server.log &"

cd /code
cd ndn-icp-download
./waf clean
./waf configure --prefix=/usr
#./waf
./waf install

cd /code
cd ndn-dash
cd libdash
mkdir build; cd build; cmake ..; make;
cd ..
cd qtsampleplayer; mkdir build; cd build; cmake .. -DLIBAV_ROOT_DIR=/usr/lib; make
