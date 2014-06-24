#!/bin/bash

# number of all clauses
C=`head -10000 $1 | grep "p " | sed -r "s/p[ ]+cnf[ ]+[0-9]+[ ]+//g"`
#echo "Number of clauses: $C"
if [ -z "$C" ]; then
  exit 0
fi
if [ $C -gt 300000 ]; then
#  echo "I guess it is not a uniform CNF formula"
  exit 0
fi

#count number of 3-clauses
C3=`cat $1 | grep -E "^[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+0" | wc -l`
#echo "Number of 3-clauses: $C3"
if [ $C3 -eq $C ]; then
#  echo "I guess it is a uniform 3-CNF formula"  
  exit 1
fi

#count number of 5-clauses
C5=`cat $1 | grep -E "^[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+0" | wc -l`
#echo "Number of 5-clauses: $C5"
if [ $C5 -eq $C ]; then
#  echo "I guess it is a uniform 5-CNF formula"  
  exit 1
fi

#count number of 7-clauses
C7=`cat $1 | grep -E "^[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+[-,0-9]+[ ]+0" | wc -l`
#echo "Number of 7-clauses: $C7"
if [ $C7 -eq $C ]; then
#  echo "I guess it is a uniform 7-CNF formula"  
  exit 1
fi

#echo "I do not know!"
exit 0
