# Setup

- Lauch:
```sudo docker-compose up --build ndn-player```


- Save
```sudo docker save ndnheadlessplayer_ndn-player > ~/containernet/docker_ndn-player.tar```

- Load
```sudo docker load --input docker_ndn-player.tar`

- Run player 
```./ndn-dash/libdash/qtsampleplayer/build/qtsampleplayer -nohead -n -bola 0.8 12 -u ndn:/n/mp```