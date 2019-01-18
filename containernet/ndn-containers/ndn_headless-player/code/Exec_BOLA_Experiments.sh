#!/bin/bash

## Launching BOLA Experiments

TARGET=(6 8) #(6 8 10 12 14 16 18 20 22)    # It can be reduced to the desired cases

SCENARIO=(ndn) #instead of (ndn tcp)

ewmaAlpha=0.8

for s in "${SCENARIO[@]}"
do
                for h in "${TARGET[@]}"
                do
                        ### From the client container  ###
                        ssh -f 10.2.0.1 -l root "source /home/ubuntu/.bashrc; killall Profile_BW*"                 #why 10.20.0.1 (eth0) instead of 10.2.0.1 (server)?
                        ssh -f 10.2.0.1 -l root "source /home/ubuntu/.bashrc; /root/Profile_BW_Shaping.sh 600 60"

                        # Launch the experiment with the given parameters
                        if [[ $s == "ndn" ]]    #NDN
                        then
                                ./executeSimpleScenario.sh -nohead -n -bola $ewmaAlpha $h -u ndn:/n/mpd
                        else                    #TCP
                                ./executeSimpleScenario.sh -nohead -bola $ewmaAlpha $h -u http://10.2.0.1/videos/BigBuckBunny.mpd
                        fi
                done
done
