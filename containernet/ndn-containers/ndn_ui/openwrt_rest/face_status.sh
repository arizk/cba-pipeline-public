#!/bin/bash

# incoming/outgoing Interest/Data counts
array=( "192.168.1.118" "192.168.1.142" "192.168.1.2" "wsclient")
for i in "${array[@]}"
do
	nfd-status -f | grep $i
done
