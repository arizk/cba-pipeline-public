#!/bin/bash


while getopts ":s:v:r:t:c:" opt; do
  case ${opt} in
    s )
      SEGMENTS=$OPTARG
      ;;
    t )
      TYPE=$OPTARG
      ;;
    v )
      VIDEOS_FOLDER=$OPTARG
      ;;
    r )
      REPO_NG_OPTIONS=$OPTARG
      ;;
    c )
      CACHESIZE=$OPTARG
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      ;;
  esac
done
shift $((OPTIND -1))

export NFD_LOG="/var/log/nfd.out"
export NFD_ERROR_LOG="/var/log/nfd-error.out"

export NDN_COPY_LOG="/var/log/copy-repo.log"
export NDN_COPY_ERROR="/var/log/copy-repo-error.log"

export NDNFS_LOG="/var/log/ndnfs.log"
export NDNFS_SERVER_LOG="/var/log/ndnfs-server.log"

export REPO_NG_LOG="/var/log/repong.log"
export REPONG_ERROR_LOG="/var/log/repong-error.log"



SEGMENTS="${SEGMENTS:-300}"
VIDEOS_FOLDER="${VIDEOS_FOLDER:-"/videos"}"
REPO_NG_OPTIONS="${REPO_NG_OPTIONS:-" -v -D -u "}"
CACHESIZE="${CACHESIZE:-"30000"}"
export SEGMENTS=$SEGMENTS
export VIDEOS_FOLDER=$VIDEOS_FOLDER
export REPO_NG_OPTIONS=$REPO_NG_OPTIONS

#Setting cache size
sed -i "s/cs_max_packets 1000/cs_max_packets ${CACHESIZE}/" /usr/local/etc/ndn/nfd.conf
echo $SEGMENTS $VIDEOS_FOLDER $REPO_NG_OPTIONS $CACHESIZE

pkill nfd
pkill ndn-repo-ng

rm ${NFD_LOG} ${NFD_ERROR_LOG} ${NDN_COPY_LOG} ${NDN_COPY_ERROR} ${NDNFS_LOG} ${NDNFS_ERROR_LOG} ${REPO_NG_LOG} ${REPONG_ERROR_LOG}

echo "Starting NFD - Named Data Networking Forwarding Daemon"

echo "Copying NFD config"
#cp ./nfd.conf /usr/local/etc/ndn/nfd.conf
setcap cap_net_raw=eip /usr/local/bin/nfd
setcap cap_net_raw,cap_net_admin=eip /usr/local/bin/nfd
nohup nfd-start > ${NFD_LOG} 2> ${NFD_ERROR_LOG} < /dev/null &

echo "Waiting for NFD to Start"
sleep 5

if [ "$TYPE" == "ndnfs" ]
then
  echo "Copy Files to NDNFS"
  nohup ./copy_files_to_ndnfs.sh > ${NDN_COPY_LOG}  2> ${NDN_COPY_ERROR} < /dev/null &
  tail -f ${NFD_LOG} ${NFD_ERROR_LOG} ${NDN_COPY_LOG} ${NDNFS_LOG} ${NDNFS_SERVER_LOG}
fi

if [ "$TYPE" == "repong-bbb" ]
then
  echo "Copy Files to Repo-NG with BBB Dataset"
  nohup ./copy_bbb_repong.sh > ${NDN_COPY_LOG}  2> ${NDN_COPY_ERROR} < /dev/null &
  tail -f ${NFD_LOG} ${NFD_ERROR_LOG} ${NDN_COPY_LOG} ${REPO_NG_LOG} ${REPONG_ERROR_LOG}
fi

if [ "$TYPE" == "repong" ]
then
  echo "Copy Files to Repo-NG"
  nohup ./copy_files_to_repong.sh > ${NDN_COPY_LOG}  2> ${NDN_COPY_ERROR} < /dev/null &
  tail -f ${NFD_LOG} ${NFD_ERROR_LOG} ${NDN_COPY_LOG} ${REPO_NG_LOG} ${REPONG_ERROR_LOG}
fi

trap "pkill nfd; pkill ndn-repo-ng; exit" SIGHUP SIGINT SIGTERM
