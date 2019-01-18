#!/bin/bash

## Launching AdapTech Experiments

LOW=(10 20 30)

HIGH=(40 60 80)

SCENARIO=(ndn)

ewmaAlpha=0.8

for s in "${SCENARIO[@]}"
do
        for l in "${LOW[@]}"
        do
                for h in "${HIGH[@]}"
                do
                        ### From the client container  ###
                        ssh -f 10.2.0.1 -l root "source /home/ubuntu/.bashrc; killall Profile_BW*"
                        ssh -f 10.2.0.1 -l root "source /home/ubuntu/.bashrc; /root/Profile_BW_Shaping.sh 600 60"

                        # Launch the experiment with the given parameters
                        if [[ $s == "ndn" ]]    #NDN
                        then
                                ./executeSimpleScenario.sh -nohead -n -br $ewmaAlpha $l $h -u ndn:/n/mpd
                        else    #TCP
                                ./executeSimpleScenario.sh -nohead -br $ewmaAlpha $l $h -u http://10.2.0.1/videos/BigBuckBunny.mpd
                        fi
                done
        done
done

