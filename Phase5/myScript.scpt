#!/bin/bash

make clean 

for i in `seq 1 4`;
do 
	if [ "$i" -lt 10 ] 
	then 
		make clean 
		testphase5.ksh 0$i | grep passed! >t0$i.out  # store outputs to a file
	else
		make clean 
		testphase5.ksh $i | grep passed! >t$i.out  # store outputs to a file
	fi
done 
