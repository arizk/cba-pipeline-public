#!/bin/bash
sudo umount /tmp/ndnfs
sudo killall ndn
sudo killall nfd
sudo rm -rf /tmp/*
sudo ./start_ndn_fs.sh
