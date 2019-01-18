#!/bin/bash
#TODO BUILD 

TARGET=(docker) #(6 8 10 12 14 16 18 20 22)    # It can be reduced to the desired cases

SCENARIO=(ndn) #instead of (ndn tcp)

ewmaAlpha=0.8

st=""
for i in "$@"
do
	if [[ $i == *"nohead"* ]] || [[ $i == "-n" ]] || [[ $i == "-u" ]] || [[ $i == *"mpd"* ]]
	then
		st=`echo $st`
	else
		if [[ $i == *"-"* ]]
			then
			st_tmp=`echo $i | cut -d "-" -f 2`
			st=`echo $st\_$st_tmp`
		else
			st=`echo $st\_$i`
		fi
	fi
done

#rm log*
#sudo tail -n 1 -f /root/log/link_$(hostname)_server.log > log$(hostname) &

#sleep $((5 * $(($(hostname | cut -dt -f2) - 1))))

sleep 1

/code/ndn-dash/libdash/qtsampleplayer/build/qtsampleplayer $@

#sudo killall tail
