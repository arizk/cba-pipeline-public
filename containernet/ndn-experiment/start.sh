#!/bin/bash

#TODO Pull ndn_repo and ndn_headless-player folders from somewhere

copyres () {
  docker cp mn.client1:/log /home/ubuntu/containernet/results/client1/$(date +%Y%m%d%T)_log; docker cp mn.client2:/log /home/ubuntu/containernet/results/client2/$(date +%Y%m%d%T)_log;
  docker cp mn.client1:/log_ChunktimeObserver /home/ubuntu/containernet/results/client1/$(date +%Y%m%d%T)_log_ChunktimeObserver; docker cp mn.client2:/log_ChunktimeObserver /home/ubuntu/containernet/results/client2/$(date +%Y%m%d%T)_log_ChunktimeObserver;
}
export -f copyres

cleandocker ()
{
  docker rm $(docker ps -a -q)
  docker rmi --force $(docker images -q)
  docker system prune --force

  # Double-tap
  systemctl stop docker
  rm -rf /var/lib/docker/aufs
  apt-get autoclean
  apt-get autoremove
  systemctl start docker
}
export -f cleandocker

# Make sure the last run has been obtained
if [ ! -z "$(ls -A /home/ubuntu/containernet/results/)" ]; then
   echo "../results directory not empty. Store the results somewhere, then do 'rm ../results/*'. Aborting."
   exit 1
fi

if [ "$1" == '--build-docker' ]
then
    echo "Build Containers"
    #docker rmi $(docker images -q)
    cd /home/ubuntu/containernet/ndn-containers/ndn_headless-player/
    docker-compose build ndn-player
    cd /home/ubuntu/containernet/ndn-containers/ndn_repo/
    docker-compose build --build-arg STORAGE_TYPE=repong-bbb ndn
fi
if [ "$1" == '--build-repo' ]
then
    echo "Build Repo"
    cd /home/ubuntu/containernet/ndn/containers/ndn_repo/
    docker-compose build --build-arg STORAGE_TYPE=repong-bbb ndn
fi
if [ "$1" == '--build-player' ]
then
    echo "Build Player"
    cd /home/ubuntu/containernet/ndn-containers/ndn_headless-player/
    docker-compose build ndn-player
fi
echo "Start Test"
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
			#echo ""
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
