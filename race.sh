#!/bin/bash

N=0
echo $#
if [ $# -lt 1 ];then
  echo "./race.sh <count>"
  exit
fi
N=$1
dir=`pwd`
start=10020
end=10022
port=0
cmd=
ip=10.0.11.212
pn=1
for ((port = $start; port < $start + $N; port++))
do
	{
	cmd="$dir/paxos_client $ip $port prepare $pn"
	pn=$(($pn + 1))
	echo $cmd
	eval $cmd
	#sleep 1
	} &
done
wait
echo "end"
