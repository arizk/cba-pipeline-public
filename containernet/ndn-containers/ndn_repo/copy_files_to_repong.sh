#!/bin/bash
cd $VIDEOS_FOLDER

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

for fil in $(find . -type f ); do
    echo ${fil:2}
    ndnputfile -v -x 3600000 /example/repo/1 /ndn/broadcast/ndnfs/videos/${fil:2}/  /videos/${fil:2}
done

echo "done."
