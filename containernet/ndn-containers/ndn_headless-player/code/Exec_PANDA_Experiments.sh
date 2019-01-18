#!/bin/bash

## Launching PANDA Experiments

TARGET=(34) #(34 44 54)

SCENARIO=(ndn) #instead of (ndn tcp)

ewmaAlpha=0.8

for s in "${SCENARIO[@]}"
do
        for h in "${TARGET[@]}"
        do
                ### From the client container  ###
                ssh -f 10.2.0.1 -l root "source /home/ubuntu/.bashrc; killall Profile_BW*"
                ssh -f 10.2.0.1 -l root "source /home/ubuntu/.bashrc; /root/Profile_BW_Shaping.sh 600 60"
                # Launch the experiment with the given parameters
                if [[ $s == "ndn" ]]    #NDN
                then
                        ./executeSimpleScenario.sh -nohead -n -p $ewmaAlpha $h -u ndn:/n/mpd
                else    #TCP
                        ./executeSimpleScenario.sh -nohead -p $ewmaAlpha $h -u http://10.2.0.1/videos/BigBuckBunny.mpd
                fi
        done
done
