#!/bin/sh

### BEGIN INIT INFO
# Provides:          ndn_file_system
# Required-Start:    $local_fs $network
# Required-Stop:     $local_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: ndn_file_system
# Description:       startup service for the ndn file system
### END INIT INFO

set -e

NAME=ndn_file_system

# location of ndnfs
directory="/home/ubuntu/Downloads/ndnfs-port-master/build"

# directory containing the files to upload after starting the server
files="/home/ubuntu/ndnfs_files/"

case "$1" in
  start)
        echo -n "Starting daemon: "$NAME
	

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

	echo "done."
        echo "."
	;;

  stop)
        echo -n "Stopping daemon: "$NAME

	nfd-stop

	killall ndnfs-server

	rm -r /tmp/ndnfs	

        echo "."
	;;

  restart)
        echo -n "Restarting daemon: "$NAME

	nfd-stop

	killall ndnfs-server

	rm -r /tmp/ndnfs	

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

	echo "."
	;;

  *)
	echo "Usage: "$1" {start|stop|restart}"
	exit 1
esac

exit 0
