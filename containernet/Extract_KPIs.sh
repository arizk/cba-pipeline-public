#!/bin/bash

InDir="$1"
logNDN="$2"

OutDir=$InDir/$logNDN\_out
if [ -d "$OutDir" ]; then rm -Rf $OutDir; fi
mkdir $OutDir

echo "$InDir"
TAB=$'\t'

####### SIMPLE SCENARIO   #######

## Client Chunk Throughput
grep Chunk-Throughput $InDir/$logNDN | awk '{print $1, $8}' > $OutDir/chunkThroughput

## Client Throughput (complete segment or sampled)
grep instant $InDir/$logNDN | awk '{print $1, $10}' | sed 's/,//' > $OutDir/throughput

## Batch BW  (Only for BOLA)
grep BATC $InDir/$logNDN > $OutDir/batch_BW

## Client Buffer Level
grep Media_object_buffer $InDir/$logNDN | grep Asking_Front | awk '{print $1, $5}' > $OutDir/bufferlevel

## Client Quality
grep DASH_Rec $InDir/$logNDN | grep -v QUALITY_SWITCH > $OutDir/tmp_quality
paste <(cat $OutDir/tmp_quality | awk '{print $1, $2, $4}') <(cat $OutDir/tmp_quality | cut -d/ -f3) > $OutDir/quality  #TODOTIMO: Update to extract real bps!
rm $OutDir/tmp_quality

## Client EWMA
grep AverageBW $InDir/$logNDN | awk '{print $1, $12}' | cut -d, -f1 > $OutDir/ewma

## Client Quality Switch
grep DASH_Receiver $InDir/$logNDN | grep -v QUAL | awk '{print $3}' | cut -d/ -f2,3 | sed 's/\// /' | tr '[a-i]' '[1-9]' | awk 'BEGIN {n = $1; m =0; counter = 1} {if(n == ""){n = $1} else {if($1 == n) {counter = counter + 1;} else { print "change:", counter, "from" ,n , "to",$1, "magnitude",sqrt((n-$1)^2); n = $1; counter = 1;m = m +1;}} } END {print "End_streak: ", counter; print "#switches:", m}' > $OutDir/tmp_qswitch
grep DASH_Receiver $InDir/$logNDN | grep -v QUAL | awk '{print $3}' | cut -d/ -f2,3 | sed 's/\// /' | tr '[a-i]' '[1-9]' | awk 'BEGIN {n = $1; m =0; counter = 1} {if($1 == n) {m = m + 0; counter = counter+1;} else {m = m+sqrt((n-$1)^2); counter = counter + 1; n=$1}} END {print "Avg Switch Magnitude:", m/(counter-1)}' >> $OutDir/tmp_qswitch
grep switches $OutDir/tmp_qswitch > $OutDir/switch
grep End $OutDir/tmp_qswitch | awk '{print "#" $1, $2}' >> $OutDir/switch
grep Avg $OutDir/tmp_qswitch | awk '{print "#" $1, $2, $3, $4}' >> $OutDir/switch
grep -vE "switches|Avg" $OutDir/tmp_qswitch | awk 'BEGIN {n = 0; m =0; z=0} {n = n + $2; z = z + $8; m = m + 1} END {print "#Mean:", (n/m)*2; print "#consecutive from to magnitude"}' >> $OutDir/switch
grep -vE "switches|End|Avg" $OutDir/tmp_qswitch >> $OutDir/switch
rm $OutDir/tmp_qswitch

## Client REBUFFERING
cat $InDir/$logNDN | grep REB | tail -n +3 > $OutDir/rebuffer
grep START $OutDir/rebuffer | awk '{print $1}' > $OutDir/rebuffer_start
grep STOP $OutDir/rebuffer | awk '{print $1}' > $OutDir/rebuffer_stop
paste $OutDir/rebuffer_start $OutDir/rebuffer_stop > $OutDir/rebuffer_time
sed -i -e "s/^/Rebuffering${TAB}&/" $OutDir/rebuffer_time
sed "s/$/${TAB}&0/" $OutDir/rebuffer_time > $OutDir/rebuffer_time_gpl_format

sed 's/\./,/' $OutDir/rebuffer_time > $OutDir/reb_tmp
sed 's/\./,/' $OutDir/reb_tmp > $OutDir/reb_tmp2

reb_num=`wc -l $OutDir/rebuffer_time | awk '{print $1}'`
reb_sum=`awk '{sum+=($3 - $2)} END {print sum}' $OutDir/reb_tmp2`
echo -e "Number of Rebuffering Events:\t$reb_num \nTotal Time Rebuffering:\t$reb_sum s" > $OutDir/rebuffer_info

rm $OutDir/rebuffer_start $OutDir/rebuffer_stop $OutDir/rebuffer $OutDir/rebuffer_time* $OutDir/reb_tmp*