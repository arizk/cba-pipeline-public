#!/bin/bash

# location of ndnfs
directory="/ndnfs-port/build"

mkdir /tmp/ndnfs
mkdir /tmp/dir

nohup $directory/ndnfs /tmp/dir /tmp/ndnfs  -o nonempty -o log=$NDNFS_LOG -o db=/tmp/ndnfs.db > $NDNFS_LOG  2> $NDNFS_LOG < /dev/null &
#I don't know why i need to pass "-o nonempty" :-/

nohup $directory/ndnfs-server -p /ndn/broadcast/ndnfs -l $NDNFS_SERVER_LOG -f /tmp/ndnfs -d /tmp/ndnfs.db > $NDNFS_SERVER_LOG  2> $NDNFS_SERVER_LOG < /dev/null &
sleep 5

echo "Uploading files ..."
cp -avr --no-preserve=mode,ownership $VIDEOS_FOLDER /tmp/ndnfs

echo "done."
