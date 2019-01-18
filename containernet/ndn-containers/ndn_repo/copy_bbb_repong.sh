#!/bin/bash
cd $VIDEOS_FOLDER
pkill ndn-repo-ng
echo "Pushing files to repo_ng"
DB="ndn_repo.db"
REPONG_DB_PATH="/var/db/ndn-repo-ng/"
FILE=${VIDEOS_FOLDER}${DB}
rm $FILE
echo "Starting Repo-ng"
ndn-repo-ng &
#redirecting repong output does not work
#> ${NDN_REPO_LOG} 2> ${NDN_REPO_ERROR} < /dev/null &
echo "Waiting to init DB"
sleep 3
# #    " Write a file into a repo.\n"
#           "  -u: unversioned: do not add a version component\n"
#           "  -s: single: do not add version or segment component, implies -u\n"
#           "  -D: use DigestSha256 signing method instead of SignatureSha256WithRsa\n"
#           "  -i: specify identity used for signing Data\n"
#           "  -I: specify identity used for signing commands\n"
#           "  -x: FreshnessPeriod in milliseconds\n"
#           "  -l: InterestLifetime in milliseconds for each command\n"
#           "  -w: timeout in milliseconds for whole process (default unlimited)\n"
#           "  -v: be verbose\n"
#           "  repo-prefix: repo command prefix\n"
#           "  ndn-name: NDN Name prefix for written Data\n"
#           "  filename: local file name; \"-\" reads from stdin\n"
#           );

RATES="quality1032682bps quality1546902bps quality2133691bps quality3078587bps quality3526922bps"
ndn_rate=0
ndn_rates_char=(a b c d e)
SEGMENTS="${SEGMENTS:-300}"
ndnputfile ${REPO_NG_OPTIONS} ndn:/localhost/repo-ng ndn:/n/mpd $VIDEOS_FOLDER/bbb/mpd
#TODO check if -D and -u options are needed (error is can not parse mpd when using it with player)
#Allow to set options when starting container
for rate in $RATES
do
    #rm -f robots.txt
    chunk_number=1

    echo "${REPO_NG_OPTIONS} ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/init $VIDEOS_FOLDER/bbb/$rate/init.mp4"
    ndnputfile ${REPO_NG_OPTIONS} ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/init $VIDEOS_FOLDER/bbb/$rate/init.mp4

    for chunk in $(seq -f "seg-%1g.m4s" $SEGMENTS)
    do
    sem -j+10  
    ndnputfile ${REPO_NG_OPTIONS} ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/$chunk_number $VIDEOS_FOLDER/bbb/$rate/$chunk 
    #echo "ndnputfile ${REPO_NG_OPTIONS} ndn:/localhost/repo-ng ndn:/n/${ndn_rates_char[$ndn_rate]}/$chunk_number $VIDEOS_FOLDER/bbb/$rate/$chunk"
    ((chunk_number++))
    done
    sem --wait
    ((ndn_rate++))
done

touch /INITDONE
