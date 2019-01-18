#!/bin/bash

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

./ndn-dash/libdash/qtsampleplayer/build/qtsampleplayer $@

if [[ $@ == *"ndn"* ]]
then
	mv log logNDN$(hostname | cut -dt -f2)$st
else
	mv log logTCP$(hostname | cut -dt -f2)$st
fi

#sudo killall tail
