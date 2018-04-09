#!/bin/bash

make clean 

for i in `seq 9 13`;
do 
	if [ "$i" -lt 10 ] 
	then 
		make clean 
		testphase4.ksh 0$i | grep failed >t0$i.out  # store outputs to a file
	else
		make clean 
		testphase4.ksh $i | grep failed >t$i.out  # store outputs to a file
	fi
done 
