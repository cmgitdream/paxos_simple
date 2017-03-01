#!/bin/bash

N=0
if [ $# -lt 2 ];then
  echo "./race.sh <ip_of_this_host> <count>"
  exit
fi
ip=$1
N=$2
dir=`pwd`
start=10020
end=10022
port=0
cmd=
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
