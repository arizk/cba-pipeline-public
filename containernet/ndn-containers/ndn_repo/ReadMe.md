Usage: 
======
This project needs docker >= 17.06.0
build project: 
```docker-compose build --build-arg STORAGE_TYPE=ndnfs ndn```
run project: 
```docker-compose up```


LEGACY DOCUMENTATION:
====================

Run: ```python setup.py -o run -p ~/videos``` to build & run ndn repo container. Were ~/videos is my video directory in the host OS
Stop Running Containers: ```python setup.py -o destroy```

The docker container can be used without this python wrapper. See docker/ReadMe for details

To open running container in interactive mode:
```docker exec -it <name-of-image>```
(You can find the name of the container from the output of run command)

This script will expose port 80 & 9696 for the host container. To make these container ports accessble to outside world, update run configuraiton in code and use -P option
https://docs.docker.com/engine/reference/run/#expose-incoming-ports

Requirements
============
Docker should be installed & running on machine

NDN-Repo-Docker-Image
=========================

Build Image:
```sudo docker run -it ndndash```

run interactive container by:
```sudo docker run -it ndndash```

run dontainer in background
```docker run -d ndndash --tty=true```

insert test file in repo:
```ndnputfile -u /example/repo/1  /example/data/1/%FD%00%00%01G%F0%C8%AD-  test.txt```


get test file:
```ndngetfile /example/data/1```

start ping server:
```ndnpingserver  ndn:/local/ &```

detailed documentation about ndn tools:
https://redmine.named-data.net/projects/repo-ng/wiki/Tools

# Using image in containernet (In Progess)

- Export Image 
```sudo docker save ndnrepo_ndn > ~/containernet/docker_ndn.tar```
 - In Vagrant:

```sudo docker load --input ../docker_ndn.tar```

- Delete all Images:
sudo docker rm $(sudo docker ps -a -q)

# Docker Compose Commands
```docker-compose up --build  ndn```
