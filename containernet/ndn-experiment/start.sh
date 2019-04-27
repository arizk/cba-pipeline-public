#!/bin/bash

# Useful functions
buildPlayer() {
    echo "Build Player"
    cd /home/ubuntu/containernet/ndn-containers/ndn_headless-player/
    docker-compose build ndn-player
}

buildRepo() {
	echo "Build Repo"
	cd /home/ubuntu/containernet/ndn-containers/ndn_repo/
	docker-compose build --build-arg STORAGE_TYPE=repong-bbb ndn
}

# Ensure results folder exists and is empty
if [ ! -d "/home/ubuntu/containernet/results/" ]; then
	mkdir /home/ubuntu/containernet/results/
fi
if [ ! -z "$(ls -A /home/ubuntu/containernet/results/)" ]; then
	echo "../results directory not empty. Store the results somewhere, then do 'rm ../results/*'. Aborting."
	exit 1
fi

# Build containers if desired
if [ "$1" == '--build-docker' ]; then
	buildPlayer
	buildRepo
fi
if [ "$1" == '--build-repo' ]; then
   buildRepo
fi
if [ "$1" == '--build-player' ]; then
	buildPlayer
fi

# Begin the evaluation
echo "Beginning evaluation"
cd /home/ubuntu/containernet/ndn-experiment

# Clear out old ucb objects
rm -rf /home/ubuntu/containernet/ucb_objs
mkdir /home/ubuntu/containernet/ucb_objs

# Run each algorithm with various parameters.
# All have topology "large-scale", duration 200s, 20 clients
declare -a qoe_weights_list=("6 2 2" "1 1 3"); # "1 1 10");
declare -a algo_list=("sbuose" "bola" "sbu" "linucb" "p");
for epoch in $(seq 1 5); do
    echo "Epoch $epoch"
    for qoe_weights in "${qoe_weights_list[@]}"; do
        mkdir -p /home/ubuntu/containernet/ucb_objs/client{1..10}/${qoe_weights// /_}
        for algo in "${algo_list[@]}"; do
            # If it's a bandit, try it with memory. Either way, also do it without.
            if [[ "$algo" =~ ^(sbu|sbuose|linucb)$ ]]; then
                docker stop $(docker ps -aq)
                docker rm $(docker ps -aq)
                mn -c
                echo "Executing $algo, QoE $qoe_weights, memory"
                python /home/ubuntu/containernet/ndn-experiment/docker_ndn.py -a "$algo" -q $(echo "$qoe_weights") -t large-scale -d 200 --memory -e "$epoch"
            fi
            docker stop $(docker ps -aq)
            docker rm $(docker ps -aq)
            mn -c
            echo "Executing $algo, QoE $qoe_weights, no memory"
            python /home/ubuntu/containernet/ndn-experiment/docker_ndn.py -a "$algo" -q $(echo "$qoe_weights") -t large-scale -d 200 --no-memory -e "$epoch"
        done
    done
done
