#!/bin/bash

# location of ndnfs
directory="/home/kom/ndnfs-port/build"

# directory containing the files to upload after starting the server
files="/home/kom/ndnfs_files/"

echo "Starting NFD - Named Data Networking Forwarding Daemon"

nfd-start

echo "Starting NDNFS"

mkdir /tmp/ndnfs
mkdir /tmp/dir
$directory/ndnfs /tmp/dir /tmp/ndnfs

echo "Starting NDNFS-server"

$directory/ndnfs-server -p /ndn/broadcast/ndnfs -l /tmp/ndnfs-server.log -f /tmp/ndnfs -d /tmp/ndnfs.db

echo "Uploading files ..."

cp -avr --no-preserve=mode,ownership $files /tmp/ndnfs

$directory/ndnfs-server -p /ndn/broadcast/ndnfs -l /tmp/ndnfs-server.log -f /tmp/ndnfs -d /tmp/ndnfs.db

echo "done."

tail -f /tmp/ndnfs-server.log
