#!/bin/bash

# incoming/outgoing Interest/Data counts
array=( "192.168.1.116" "192.168.1.190" "192.168.1.1:6363" "wsclient" )
for i in "${array[@]}"
do
	nfd-status -f | grep $i
done
